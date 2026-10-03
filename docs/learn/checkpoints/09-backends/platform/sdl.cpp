#include "platform/platform.h"
#include "platform/text_queue.h"
#include <SDL.h>
#include <array>
#include <cstddef>
#include <cstdio>
#include <exception>
#include <memory>
#include <limits>
#include <utility>

namespace {
// This teaching executable is the sole owner of SDL's global lifetime.
class SdlRuntime {
public:
    SdlRuntime() {
        SDL_SetMainReady();
        ready_ = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
    }
    ~SdlRuntime() noexcept { SDL_Quit(); }
    SdlRuntime(const SdlRuntime&) = delete;
    SdlRuntime& operator=(const SdlRuntime&) = delete;
    bool ready() const noexcept { return ready_; }
private:
    bool ready_;
};

struct WindowDeleter {
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};
using WindowOwner = std::unique_ptr<SDL_Window, WindowDeleter>;

constexpr std::size_t key_count = static_cast<std::size_t>(Key::Count);
Key map_key(SDL_Keycode code)
{
    switch (code) {
    case SDLK_LEFT: return Key::Left;
    case SDLK_RIGHT: return Key::Right;
    case SDLK_ESCAPE: return Key::Escape;
    default: return Key::Count;
    }
}

struct Resources {
    // Members are destroyed in reverse declaration order: window before runtime.
    SdlRuntime runtime;
    WindowOwner window;
    Uint64 frequency = 0;
    Uint64 previous = 0;
    Uint64 frame_start = 0;
    bool should_close = false;
    std::array<bool, key_count> current{};
    std::array<bool, key_count> previous_keys{};
    AsciiQueue<64> text;
    std::uint64_t dropped = 0;
    bool text_started = false;
    ~Resources() noexcept
    {
        // Destructor body runs before members: the window and SDL still exist.
        if (text_started) SDL_StopTextInput();
    }
};
std::unique_ptr<Resources> active;
}

bool platform_init(int width, int height, const char* title)
{
    if (active) {
        std::fputs("already initialized\n", stderr);
        return false;
    }
    try {
        auto candidate = std::make_unique<Resources>();
        if (!candidate->runtime.ready()) {
            std::fprintf(stderr, "init failed: %s\n", SDL_GetError());
            return false;
        }
        candidate->window.reset(SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width, height, 0));
        if (!candidate->window) {
            std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
            return false;
        }
        candidate->frequency = SDL_GetPerformanceFrequency();
        if (candidate->frequency == 0) {
            std::fputs("performance frequency is zero\n", stderr);
            return false;
        }
        candidate->previous = candidate->frame_start = SDL_GetPerformanceCounter();
        SDL_StartTextInput();
        candidate->text_started = true;
        active = std::move(candidate); // Publish only a fully initialized owner.
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "initialization exception: %s\n", error.what());
        return false;
    }
}

void platform_shutdown() noexcept { active.reset(); }
bool platform_should_close() { return !active || active->should_close; }

FrameInfo platform_begin_frame()
{
    if (!active) return {};
    auto& state = *active;
    state.frame_start = SDL_GetPerformanceCounter();
    FrameInfo frame{static_cast<double>(state.frame_start - state.previous)
                    / static_cast<double>(state.frequency), 0};
    state.previous = state.frame_start;
    state.previous_keys = state.current; // Snapshot BEFORE applying new events.
    SDL_Event event{};
    while (SDL_PollEvent(&event) != 0) {
        ++frame.events;
        if (event.type == SDL_QUIT) {
            state.should_close = true;
            break;
        }
        if (event.type == SDL_TEXTINPUT) {
            for (const unsigned char byte : event.text.text) {
                if (byte == 0) break;
                if (byte >= 128) continue; // ASCII-only contract, not UTF-8 decoding.
                if (!state.text.push(byte) && state.dropped < std::numeric_limits<std::uint64_t>::max())
                    ++state.dropped;
            }
        } else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            const auto index = static_cast<std::size_t>(map_key(event.key.keysym.sym));
            if (index < key_count) state.current[index] = event.type == SDL_KEYDOWN;
        } else if (event.type == SDL_WINDOWEVENT &&
                   event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
            // Policy: cancel held controls on focus loss; retain previous for release edges.
            state.current.fill(false);
        }
    }
    return frame;
}

void platform_end_frame()
{
    if (!active || active->should_close) return;
    const auto& state = *active;
    const double work_seconds = static_cast<double>(SDL_GetPerformanceCounter() - state.frame_start)
                              / static_cast<double>(state.frequency);
    const double remaining = (1.0 / 60.0) - work_seconds;
    if (remaining >= 0.001) SDL_Delay(static_cast<Uint32>(remaining * 1000.0));
}

bool platform_key_down(Key key)
{
    const auto i = static_cast<std::size_t>(key);
    return active && i < key_count && active->current[i];
}
bool platform_key_pressed(Key key)
{
    const auto i = static_cast<std::size_t>(key);
    return active && i < key_count && active->current[i] && !active->previous_keys[i];
}
bool platform_key_released(Key key)
{
    const auto i = static_cast<std::size_t>(key);
    return active && i < key_count && !active->current[i] && active->previous_keys[i];
}

char platform_get_char_pressed() { return active ? active->text.pop() : 0; }
std::uint64_t platform_text_dropped() { return active ? active->dropped : 0; }
