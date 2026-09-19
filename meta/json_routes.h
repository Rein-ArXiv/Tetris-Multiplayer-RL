#pragma once
#include "json_input.h"
#include "httplib.h"
#include <utility>

namespace meta {
// cpp-httplib's pre-routing hook runs before reading the request body. Validate
// here, after its bounded body reader and before any route can mutate state.
inline void json_post(httplib::Server &server, const std::string &path, httplib::Server::Handler handler) {
    server.Post(path, [handler = std::move(handler)](const httplib::Request &req, httplib::Response &res) {
        if (!json_input::object(req.body)) {
            res.status = 400;
            res.set_header("Cache-Control", "no-store");
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_content("{\"error\":\"invalid_json\"}", "application/json");
            return;
        }
        handler(req, res);
    });
}
} // namespace meta
