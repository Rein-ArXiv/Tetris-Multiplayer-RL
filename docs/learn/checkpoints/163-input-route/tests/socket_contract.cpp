#include "net/socket.h"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#include <utility>
#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
    using namespace study_net;
    static_assert(!std::is_copy_constructible_v<Socket>);
    static_assert(std::is_nothrow_move_constructible_v<Socket>);
    Runtime runtime;
    CHECK(runtime.ready());
    CHECK(Runtime::native_valid(0));
    CHECK(!Runtime::native_valid(Runtime::kInvalid));
    int error = 0;
    CHECK(!connect_loopback(0,error).valid() && error != 0);
    Socket invalid; bool eof = true; std::uint8_t byte = 99;
    CHECK(!receive_byte(invalid,byte,eof,error) && !eof && error != 0 && byte == 99);
    CHECK(!send_byte(invalid,42,error) && error != 0);
    std::uint16_t port = 0;
    Socket listener = listen_loopback(0,port,error);
    CHECK(listener.valid() && port != 0 && error == 0);
    // The kernel queues a connection before the application calls accept.
    Socket client = connect_loopback(port,error);
    CHECK(client.valid());
    Socket peer = accept_one(listener,error);
    CHECK(peer.valid() && peer.native() != listener.native());
    listener.reset();
    CHECK(!listener.valid());
    CHECK(send_byte(client,255,error));
    CHECK(receive_byte(peer,byte,eof,error) && byte == 255 && !eof);
    Socket moved(std::move(peer));
    CHECK(!peer.valid() && moved.valid());
    Socket replacement;
    replacement = std::move(moved);
    CHECK(!moved.valid() && replacement.valid());
    CHECK(send_byte(replacement,0,error));
    CHECK(receive_byte(client,byte,eof,error) && byte == 0);
    replacement.reset();
    CHECK(!receive_byte(client,byte,eof,error) && eof && error == 0);
#ifndef _WIN32
    CHECK(!Runtime::native_valid(-2));
    int pair[2]; CHECK(::socketpair(AF_UNIX,SOCK_STREAM,0,pair) == 0);
    Socket first(pair[0]), other(pair[1]);
    const int overwritten = first.native();
    first = std::move(other);
    CHECK(::fcntl(overwritten,F_GETFD) == -1 && errno == EBADF);
    // Sending after the peer descriptor has closed must report an error, not SIGPIPE.
    CHECK(!send_byte(first,42,error) && error != 0);
    const int saved = ::dup(STDIN_FILENO);
    CHECK(::dup2(first.native(),STDIN_FILENO) == 0);
    {
        Socket zero(0);
        CHECK(zero.valid());
        zero.reset(); zero.reset();
        CHECK(::fcntl(0,F_GETFD) == -1 && errno == EBADF);
    }
    if (saved >= 0) { CHECK(::dup2(saved,0) == 0); ::close(saved); }
#endif
    std::puts("Socket lifetime, moves, descriptor zero, byte range and EOF passed.");
}
