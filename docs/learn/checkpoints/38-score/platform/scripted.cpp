#include "platform/platform.h"
#include "platform/text_queue.h"
#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <optional>

namespace {
constexpr auto key_count = static_cast<std::size_t>(Key::Count);
struct ScriptFrame {
    Key key;
    bool down;
    const char* text;
    bool quit;
};
// One text string represents one event, even if it contains several bytes.
constexpr std::array<ScriptFrame, 6> script{{
    {Key::Left,   true,  "A",  false},
    {Key::Count,  false, "",   false},
    {Key::Left,   false, "BC", false},
    {Key::Count,  false, "",   false},
    {Key::Escape, true,  "",   false},
    {Key::Count,  false, "",   true},
}};
struct State {
    std::array<bool, key_count> current{};
    std::array<bool, key_count> previous{};
    AsciiQueue<64> text;
    std::uint64_t dropped = 0;
    std::size_t frame = 0;
    bool should_close = false;
};
std::optional<State> active;
}

bool platform_init(int width, int height, const char* title)
{
    // Signature compatibility is not evidence that a real window was created.
    (void)width; (void)height; (void)title;
    if (active) {
        std::fputs("already initialized\n", stderr);
        return false;
    }
    active.emplace();
    return true;
}
void platform_shutdown() noexcept { active.reset(); }
bool platform_should_close() { return !active || active->should_close; }

FrameInfo platform_begin_frame()
{
    if (!active || active->should_close) return {};
    auto& state = *active;
    state.previous = state.current;
    const auto& input = script[state.frame];
    FrameInfo frame{0.25, 0}; // Synthetic seconds, not elapsed wall-clock time.
    if (input.quit) {
        state.should_close = true;
        ++frame.events;
        return frame;
    }
    const auto i = static_cast<std::size_t>(input.key);
    if (i < key_count) {
        state.current[i] = input.down;
        ++frame.events;
    }
    if (input.text[0]) ++frame.events;
    for (const char* p = input.text; *p; ++p) {
        const auto byte = static_cast<unsigned char>(*p);
        if (byte >= 128) continue;
        if (!state.text.push(byte) && state.dropped < std::numeric_limits<std::uint64_t>::max())
            ++state.dropped;
    }
    return frame;
}
void platform_end_frame()
{
    if (!active || active->should_close) return;
    // Normal use pairs begin_frame with end_frame; main can also terminate early.
    if (++active->frame == script.size()) active->should_close = true;
}
bool platform_key_down(Key key)
{
    const auto i = static_cast<std::size_t>(key);
    return active && i < key_count && active->current[i];
}
bool platform_key_pressed(Key key)
{
    const auto i = static_cast<std::size_t>(key);
    return active && i < key_count && active->current[i] && !active->previous[i];
}
bool platform_key_released(Key key)
{
    const auto i = static_cast<std::size_t>(key);
    return active && i < key_count && !active->current[i] && active->previous[i];
}
char platform_get_char_pressed() { return active ? active->text.pop() : 0; }
std::uint64_t platform_text_dropped() { return active ? active->dropped : 0; }

// Input-only teaching backend: no GL context to supply addresses for.
void* platform_gl_get_proc(const char*) { return nullptr; }

DrawableSize platform_drawable_size() { return {}; }
SwapIntervalReport platform_set_swap_interval(int) { return {}; }
bool platform_present() { return false; }

WindowSize platform_window_size() { return {}; }
WindowMouse platform_window_mouse() { return {}; }
