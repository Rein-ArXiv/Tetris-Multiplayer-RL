#pragma once
#include "meta/sqlite_results.h"
#include "httplib.h"
namespace study_meta {
inline void ranking_route(httplib::Server& http,SqliteResults& store) {
    http.Get("/study/v1/ranking",[&store](const httplib::Request& req,httplib::Response& res) {
        res.set_header("Cache-Control","no-store");
        if(!req.params.empty() || !req.body.empty()) {
            res.status=400;res.set_content("{\"error\":\"invalid_request\"}","application/json");return;
        }
        try {res.set_content(ranking_json(store.ranking()).dump(),"application/json");}
        catch(const std::exception&) {
            res.status=503;res.set_content("{\"error\":\"ranking_unavailable\"}","application/json");
        }
    });
}
} // namespace study_meta
