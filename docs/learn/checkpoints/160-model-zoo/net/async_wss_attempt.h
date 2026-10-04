#ifndef STUDY_ASYNC_NET_ASYNC_WSS_ATTEMPT_H
#define STUDY_ASYNC_NET_ASYNC_WSS_ATTEMPT_H

// Teaching example: exactly one asynchronous WSS echo attempt.
//
// Contract exercised here:
//   * Single-threaded only. The owner and every method run on the one thread
//     that calls io_context::run(). There is no strand and no multithreaded
//     support, so member state is protected by serialization, not locks.
//   * The io_context and the externally-owned asio::ssl::context must outlive
//     every operation started here.
//   * Every asynchronous completion captures a shared_ptr to *this, so the
//     object lives while any pending handler retains it. This protects
//     lifetime, NOT concurrent state.
//   * finish() is logical termination: it records the first End, cancels all
//     outstanding I/O, and lets the resulting completions arrive as late
//     callbacks that are counted and ignored.
//   * The owner releases its owning shared_ptr, runs the io_context until it
//     drains, then observes that its weak_ptr has expired. After io.run only
//     the Trace is meaningful data.
//   * A pending lambda self reference is released when it is dispatched or
//     when the queue is destroyed; a long-running operation holds the Attempt
//     alive until the deadline (or an explicit cancellation) aborts it.
//   * The cancel_at hook is a local diagnostic only and changes no security
//     policy.
//   * No secrets are logged or stored beyond the caller-provided Options.
//
// Host matching is delegated to net/tls_identity.h; it is not reimplemented.

#include "net/tls_identity.h"

#include <algorithm>
#include <chrono>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <openssl/ssl.h>
#include <openssl/x509.h>

namespace study_async {

enum class Stage { idle, resolve, connect, tls, upgrade, write, read };

enum class End { success, cancelled, deadline, io_error, protocol_error };

struct Trace {
    Stage stage = Stage::idle;
    std::optional<End> result;
    std::size_t reports = 0;
    std::size_t callbacks = 0;
    std::size_t late_callbacks = 0;
    std::size_t destroyed = 0;
};

// All fields are explicit experiment controls; there are no magic policy
// defaults (for example, message_limit must be chosen by the caller).
struct Options {
    std::string host;
    std::string port;
    std::vector<std::uint8_t> payload;
    std::chrono::milliseconds timeout;
    std::optional<Stage> cancel_at;
    std::size_t message_limit;
};

class Attempt : public std::enable_shared_from_this<Attempt> {
    using ws_type = boost::beast::websocket::stream<
        boost::beast::ssl_stream<boost::beast::tcp_stream>, false>;

public:
    Attempt(boost::asio::io_context& io,
            boost::asio::ssl::context& tls,
            Options options,
            std::shared_ptr<Trace> trace)
        : io_(io),
          trace_(std::move(trace)),
          options_(std::move(options)),
          resolver_(io),
          ws_(io, tls),
          timer_(io) {
        if (!trace_) {
            throw std::invalid_argument("async_wss_attempt: null trace");
        }
        validate(options_);
        configure_tls();
    }

    ~Attempt() {
        // Never use shared_from_this() here; teardown must not create owners.
        ++trace_->destroyed;
    }

    Attempt(Attempt const&) = delete;
    Attempt& operator=(Attempt const&) = delete;

    // Starts the one attempt. Idempotent: does nothing unless idle.
    void begin() {
        if (trace_->result.has_value() || trace_->stage != Stage::idle) {
            return;
        }
        const auto now = std::chrono::steady_clock::now();
        const auto max_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::duration::max());
        if (options_.timeout > max_duration ||
            now > std::chrono::steady_clock::time_point::max() - options_.timeout) {
            throw std::invalid_argument("async_wss_attempt: deadline overflow");
        }
        enter(Stage::resolve);
        timer_.expires_at(now + options_.timeout);
        timer_.async_wait(
            [self = shared_from_this()](boost::system::error_code ec) {
                self->on_deadline(ec);
            });
        resolver_.async_resolve(
            options_.host, options_.port,
            [self = shared_from_this()](
                boost::system::error_code ec,
                boost::asio::ip::tcp::resolver::results_type results) {
                self->on_resolve(ec, std::move(results));
            });
    }

    // Requests logical cancellation. Always posted so it is ordered on the io
    // thread. The first terminal action that executes wins; posting is not
    // a promise to outrun an I/O completion already becoming ready.
    void request_cancel() {
        auto self = shared_from_this();
        boost::asio::post(io_, [self]() { self->finish(End::cancelled); });
    }

private:
    static void validate(Options const& o) {
        if (o.host.empty() || o.host.find('\0') != std::string::npos ||
            o.host.find_first_of(" \r\n\t/?#@[]") != std::string::npos) {
            throw std::invalid_argument("async_wss_attempt: empty host");
        }
        unsigned port = 0;
        const auto parsed = std::from_chars(o.port.data(), o.port.data()+o.port.size(), port);
        if (parsed.ec != std::errc{} || parsed.ptr != o.port.data()+o.port.size() ||
            port == 0 || port > 65535) {
            throw std::invalid_argument("async_wss_attempt: empty port");
        }
        if (o.payload.empty()) {
            throw std::invalid_argument("async_wss_attempt: empty payload");
        }
        if (o.timeout <= std::chrono::milliseconds::zero()) {
            throw std::invalid_argument("async_wss_attempt: non-positive timeout");
        }
        if (o.message_limit == 0) {
            throw std::invalid_argument("async_wss_attempt: zero message_limit");
        }
    }

    void configure_tls() {
        SSL* ssl = ws_.next_layer().native_handle();

        if (SSL_set_min_proto_version(ssl, TLS1_2_VERSION) != 1) {
            throw std::runtime_error("async_wss_attempt: cannot require TLS 1.2");
        }

        // SNI is only meaningful for DNS names, never for IP literals.
        boost::system::error_code ip_ec;
        boost::asio::ip::make_address(options_.host, ip_ec);
        if (ip_ec) {
            if (SSL_set_tlsext_host_name(ssl, options_.host.c_str()) != 1) {
                throw std::runtime_error("async_wss_attempt: cannot set SNI");
            }
        }

        ws_.next_layer().set_verify_mode(boost::asio::ssl::verify_peer);

        // Capture the host BY VALUE. This must not capture self: the callback
        // is owned by OpenSSL and is independent of any one member operation.
        std::string const host_copy = options_.host;
        ws_.next_layer().set_verify_callback(
            [host_copy](bool preverified, boost::asio::ssl::verify_context& ctx) -> bool {
                if (!preverified) {
                    return false;
                }
                X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
                if (cert == nullptr) {
                    return false;
                }
                int const cert_depth =
                    X509_STORE_CTX_get_error_depth(ctx.native_handle());
                return cert_depth > 0 ||
                       study_net::tls_identity::matches_host(cert, host_copy);
            });
    }

    // Records the stage and optionally triggers the diagnostic cancel hook.
    // The cancel is posted and the caller starts the corresponding async op
    // afterwards, so cancellation can only run once the op is queued.
    void enter(Stage stage) {
        trace_->stage = stage;
        if (options_.cancel_at.has_value() && *options_.cancel_at == stage) {
            request_cancel();
        }
    }

    void on_resolve(boost::system::error_code ec,
                    boost::asio::ip::tcp::resolver::results_type results) {
        if (!accept(ec)) {
            return;
        }
        enter(Stage::connect);
        // The endpoint-sequence async_connect overload retains its sequence;
        // this local results object may leave scope after initiation.
        boost::asio::async_connect(
            boost::beast::get_lowest_layer(ws_).socket(), results,
            [self = shared_from_this()](
                boost::system::error_code e,
                boost::asio::ip::tcp::endpoint const&) {
                self->on_connect(e);
            });
    }

    void on_connect(boost::system::error_code ec) {
        if (!accept(ec)) {
            return;
        }
        enter(Stage::tls);
        ws_.next_layer().async_handshake(
            boost::asio::ssl::stream_base::client,
            [self = shared_from_this()](boost::system::error_code e) {
                self->on_tls(e);
            });
    }

    void on_tls(boost::system::error_code ec) {
        if (!accept(ec)) {
            return;
        }
        enter(Stage::upgrade);
        // Request target /play with the authority (host:port) as Host header.
        const auto host = options_.host.find(':') == std::string::npos
            ? options_.host : "[" + options_.host + "]";
        std::string const authority = host + ":" + options_.port;
        ws_.async_handshake(
            authority, "/play",
            [self = shared_from_this()](boost::system::error_code e) {
                self->on_upgrade(e);
            });
    }

    void on_upgrade(boost::system::error_code ec) {
        if (!accept(ec)) {
            return;
        }
        ws_.read_message_max(options_.message_limit);
        ws_.binary(true);
        enter(Stage::write);
        // options_.payload is a member and owns the write buffer, so the buffer
        // outlives the single outstanding async_write.
        ws_.async_write(
            boost::asio::buffer(options_.payload),
            [self = shared_from_this()](boost::system::error_code e, std::size_t) {
                self->on_write(e);
            });
    }

    void on_write(boost::system::error_code ec) {
        if (!accept(ec)) {
            return;
        }
        enter(Stage::read);
        start_read();
    }

    void start_read() {
        ws_.async_read(
            input_,
            [self = shared_from_this()](boost::system::error_code e, std::size_t) {
                self->on_read(e);
            });
    }

    void on_read(boost::system::error_code ec) {
        if (!accept(ec)) {
            return;
        }
        if (!ws_.got_binary()) {
            finish(End::protocol_error);  // text or unexpected frame type
            return;
        }

        // input_ owns message bytes; data() returns non-owning descriptors.
        // Copy into accumulated_ before consuming the message buffer.
        auto data = input_.data();
        for (auto it = boost::asio::buffer_sequence_begin(data);
             it != boost::asio::buffer_sequence_end(data); ++it) {
            auto const* bytes = static_cast<std::uint8_t const*>(it->data());
            std::size_t const n = it->size();
            if (n > options_.payload.size() - accumulated_.size()) {
                input_.consume(input_.size());
                finish(End::protocol_error);  // oversized echo
                return;
            }
            accumulated_.insert(accumulated_.end(), bytes, bytes + n);
        }
        input_.consume(input_.size());

        if (!std::equal(accumulated_.begin(), accumulated_.end(),
                        options_.payload.begin())) {
            finish(End::protocol_error);  // wrong bytes
            return;
        }
        if (accumulated_.size() == options_.payload.size()) {
            finish(End::success);
            return;
        }
        start_read();
    }

    void on_deadline(boost::system::error_code ec) {
        // The deadline timer counts callbacks, but unlike accept() it can never
        // turn an aborted wait into success.
        ++trace_->callbacks;
        if (trace_->result.has_value()) {
            ++trace_->late_callbacks;
            return;
        }
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                finish(End::io_error);
            }
            return;
        }
        finish(End::deadline);
    }

    // Common completion gate for every I/O operation.
    bool accept(boost::system::error_code ec) {
        ++trace_->callbacks;
        if (trace_->result.has_value()) {
            ++trace_->late_callbacks;
            return false;
        }
        if (ec) {
            finish(End::io_error);
            return false;
        }
        return true;
    }

    // Logical termination. Timeout and cancellation may lose the race to a
    // real completion; the first terminal action on the serialized run thread
    // wins, and later completions become counted late callbacks.
    void finish(End end) {
        if (trace_->result.has_value()) {
            return;
        }
        trace_->result = end;
        ++trace_->reports;

        boost::system::error_code ec;
        resolver_.cancel();
        timer_.cancel(ec);
        auto& socket = boost::beast::get_lowest_layer(ws_).socket();
        socket.cancel(ec);
        ec.clear();
        socket.close(ec);

        // input_ and options_.payload stay alive: pending handlers may still
        // reference them, and payload owns the write buffer.
    }

    boost::asio::io_context& io_;
    std::shared_ptr<Trace> trace_;
    Options options_;
    boost::asio::ip::tcp::resolver resolver_;
    ws_type ws_;
    boost::asio::steady_timer timer_;
    boost::beast::flat_buffer input_;
    std::vector<std::uint8_t> accumulated_;
};

}  // namespace study_async

#endif  // STUDY_ASYNC_NET_ASYNC_WSS_ATTEMPT_H
