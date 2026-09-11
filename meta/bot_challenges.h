#pragma once
#include <functional>
#include <optional>
#include <string>
namespace httplib { class Server; }
namespace meta {
class Database;
void register_bot_challenges(httplib::Server& server, Database& db,
    std::function<std::optional<std::string>()> secure_id);
}
