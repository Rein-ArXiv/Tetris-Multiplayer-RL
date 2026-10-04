#include "httplib.h"
#include <iostream>
#include <stdexcept>
#include <string>

static void check(bool ok) {
    if (!ok) throw std::runtime_error("early HTTP response contract failed");
}

static void rejected(const httplib::Headers& headers, size_t cap = 65536) {
    httplib::Request request;
    request.headers = headers;
    httplib::detail::BufferStream stream;
    stream.write("next", 4);
    check(!httplib::detail::discard_handled_request_body(stream, request, cap));
    char data[4];
    check(stream.read(data, 4) == 4 && std::string(data, 4) == "next");
}

int main() {
    for (const auto length : {size_t{0}, size_t{1}, size_t{65536}}) {
        httplib::Request request;
        request.set_header("Content-Length", std::to_string(length));
        httplib::detail::BufferStream stream;
        const std::string input = std::string(length, 'x') + "GET";
        stream.write(input.data(), input.size());
        check(httplib::detail::discard_handled_request_body(stream, request, 65536));
        char next[3];
        check(stream.read(next, 3) == 3 && std::string(next, 3) == "GET");
    }
    for (const std::string value : {"", "-1", "+1", "1x", " 1", "65537",
                                   "184467440737095516160000000000000000"}) {
        rejected({{"Content-Length", value}});
    }
    rejected({{"Content-Length", "1"}, {"Content-Length", "1"}});
    rejected({{"Transfer-Encoding", "chunked"}});
    rejected({{"Transfer-Encoding", "chunked"}, {"Content-Length", "1"}});
    rejected({{"Content-Length", "8"}}, 7);
    rejected({{"Content-Length", "1"}}, 0);
    httplib::Request missing;
    httplib::detail::BufferStream empty;
    check(httplib::detail::discard_handled_request_body(empty, missing, 0));
    missing.set_header("Content-Length", "2");
    check(!httplib::detail::discard_handled_request_body(empty, missing, 65536));
    std::cout << "Early-response framing and bounded discard passed\n";
}
