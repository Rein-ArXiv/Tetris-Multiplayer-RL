#pragma once

namespace study_app {

enum class Screen { menu, playing, finished, quitting };
enum class Command { none, confirm, back };

struct Transition {
    Screen next;
    bool recreate = false;
    bool release = false;
};

// Pure transition decision. The caller owns Game creation, timing, input, and OS/GL.
inline constexpr Transition transition(Screen current, Command command) noexcept {
    // Quitting is terminal: unchanged for every command.
    if (current == Screen::quitting) {
        return Transition{Screen::quitting, false, false};
    }

    switch (command) {
        case Command::back:
            // From menu, back quits; from playing/finished, back returns to menu.
            if (current == Screen::menu) {
                return Transition{Screen::quitting, false, true};
            }
            return Transition{Screen::menu, false, true};

        case Command::confirm:
            // Menu/finished start a session (recreate); playing keeps its state.
            if (current == Screen::menu || current == Screen::finished) {
                return Transition{Screen::playing, true, false};
            }
            return Transition{Screen::playing, false, false};

        case Command::none:
        default:
            return Transition{current, false, false};
    }
}

}  // namespace study_app
