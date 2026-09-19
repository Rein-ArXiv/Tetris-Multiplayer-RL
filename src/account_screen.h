#pragma once
#include <string>

struct AccountScreen {
    std::string status;
    std::string confirmation;
    // Returns an action name; the screen never performs HTTP or credential I/O.
    std::string draw(bool busy, bool unsaved, const std::string &server, const std::string &folder);
};
