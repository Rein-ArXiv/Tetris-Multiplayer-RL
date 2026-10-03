#include "account_screen.h"
#include <iterator>
#include "gui.h"
#include "../renderer/renderer.h"
#include "../platform/platform.h"

std::string AccountScreen::draw(bool busy, bool unsaved, const std::string &server,
                                const std::string &folder) {
    gui_text_center(360, 60, "Account & Recovery", 36, WHITE);
    gui_text_center(360, 105, "Keep your progress without a sign-up.", 18, GRAY);
    gui_text_center(360, 130, server.c_str(), 14, GRAY);
    gui_text_center(360, 157, busy ? "Processing account request..." : status.c_str(), 14, unsaved ? RED : GRAY);
    struct AccountAction {
        const char *label;
        const char *action;
        bool reconnect;
    };
    constexpr AccountAction actions[] = {
        {"Save recovery file", "backup", false},
        {"Replace access keys", "rotate", false},
        {"Restore recovery file", "recover", false},
        {"Retry / reconnect", "resume", true},
        {"Import older account", "import", false},
        {"Create separate account", "create", false},
    };
    for (int i = 0; i < static_cast<int>(std::size(actions)); ++i) {
        if (!gui_button(60 + (i % 2) * 310, 194 + (i / 2) * 47, 290, 36, actions[i].label, 18) || busy)
            continue;
        if (actions[i].reconnect)
            return unsaved ? "save" : "resume";
        if (unsaved)
            status = "Save the current key with Retry before changing accounts.";
        else
            confirmation = actions[i].action;
    }
    if (unsaved)
        gui_text_center(360, 348, "Online rewards are disabled until the key is saved.", 16, RED);
    if (!confirmation.empty() && !busy) {
        const char *prompt =
            confirmation == "backup"    ? "Previous recovery files will stop working. Continue?"
            : confirmation == "rotate"  ? "Revoke old keys; current matches may continue. Confirm?"
            : confirmation == "recover" ? "Switch to the account in your recovery file?"
            : confirmation == "import"  ? "Send the older saved key to the server shown above?"
                                        : "Create a separate account here and keep the older files?";
        gui_text_center(360, 394, prompt, 15, WHITE);
        if (gui_button(185, 421, 165, 34, "Confirm")) {
            auto action = confirmation;
            confirmation.clear();
            return action;
        }
        if (gui_button(370, 421, 165, 34, "Cancel"))
            confirmation.clear();
    }
    gui_text_center(360, 478, "Copy the newest account-recovery.json somewhere safe.", 16, GRAY);
    gui_text_center(360, 500, "To restore, put the recovery file in this server's folder:", 16, GRAY);
    gui_text_center(360, 524, folder.c_str(), 12, GRAY);
    if ((gui_button(260, 560, 200, 38, "Back") || platform_key_pressed(PKEY_Q) ||
         platform_key_pressed(PKEY_ESCAPE)) &&
        !busy) {
        confirmation.clear();
        return "back";
    }
    return {};
}
