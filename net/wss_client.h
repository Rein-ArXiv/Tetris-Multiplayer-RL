#pragma once
#include "socket.h"
#include <string>

namespace net {
struct WssEndpoint { std::string host, port, authority, target; };
bool parse_wss_endpoint(const std::string& url, WssEndpoint& out);
TcpSocket wss_connect(const std::string& url);
TcpSocket game_connect(const std::string& host, uint16_t port);
bool secure_game_endpoint(const std::string& host);
}
