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

// SDL_GLContext is a void*. Owning it with unique_ptr<void, ...> keeps the
// exact SDL lifetime rule: SDL_GL_DeleteContext, never delete.
struct GlContextDeleter {
    void operator()(void* context) const noexcept { SDL_GL_DeleteContext(context); }
};
using GlContextOwner = std::unique_ptr<void, GlContextDeleter>;

constexpr std::size_t key_count = static_cast<std::size_t>(Key::Count);
Key map_key(SDL_Keycode code)
{
    switch (code) {
    case SDLK_LEFT: return Key::Left;
    case SDLK_RIGHT: return Key::Right;
    case SDLK_ESCAPE: return Key::Escape;
    case SDLK_UP: return Key::Up;
    case SDLK_DOWN: return Key::Down;
    default: return Key::Count;
    }
}

// Request a 3.3 core, double-buffered context. Attributes must be set before
// SDL_CreateWindow, because window creation can lock the pixel format.
bool request_gl_attributes()
{
    const bool base =
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) == 0 &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) == 0 &&
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE) == 0 &&
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) == 0;
#ifdef __APPLE__
    if (!base) return false; // Preserve the first SDL error for the caller.
    // Request a forward-compatible core context on macOS.
    const bool forward = SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG) == 0;
#else
    const bool forward = true;
#endif
    return base && forward;
}

struct Resources {
    // Destroyed in reverse declaration order: context, then window, then
    // SDL_Quit. GL dies before the window it targeted, the window before SDL.
    SdlRuntime runtime;
    WindowOwner window;
    GlContextOwner context;
    int gl_major = 0;
    int gl_minor = 0;
    int gl_profile = 0;
    int gl_doublebuffer = 0;
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
        if (!request_gl_attributes()) {
            std::fprintf(stderr, "gl attribute request failed: %s\n", SDL_GetError());
            return false;
        }
        candidate->window.reset(SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width, height,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
        if (!candidate->window) {
            std::fprintf(stderr, "window failed: %s\n", SDL_GetError());
            return false;
        }
        candidate->context.reset(SDL_GL_CreateContext(candidate->window.get()));
        if (!candidate->context) {
            std::fprintf(stderr, "gl context failed: %s\n", SDL_GetError());
            return false;
        }
        // SDL2 docs say CreateContext makes the context current; verify that
        // with SDL's own accessors instead of trusting an implicit side effect.
        if (SDL_GL_GetCurrentContext() != candidate->context.get()) {
            std::fputs("gl context is not current\n", stderr);
            return false;
        }
        if (SDL_GL_GetCurrentWindow() != candidate->window.get()) {
            std::fputs("gl current window mismatch\n", stderr);
            return false;
        }
        // SDL reports its GL configuration; version/profile may be cached requests.
        // Direct GL queries in the next layer verify the driver context.
        if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &candidate->gl_major) != 0 ||
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &candidate->gl_minor) != 0 ||
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &candidate->gl_profile) != 0 ||
            SDL_GL_GetAttribute(SDL_GL_DOUBLEBUFFER, &candidate->gl_doublebuffer) != 0) {
            std::fprintf(stderr, "gl attribute query failed: %s\n", SDL_GetError());
            return false;
        }
        const bool version_ok = candidate->gl_major > 3 ||
            (candidate->gl_major == 3 && candidate->gl_minor >= 3);
        if (!version_ok ||
            candidate->gl_profile != SDL_GL_CONTEXT_PROFILE_CORE ||
            candidate->gl_doublebuffer != 1) {
            std::fprintf(stderr,
                "unsupported SDL GL config: %d.%d profile=%d doublebuffer=%d\n",
                candidate->gl_major, candidate->gl_minor,
                candidate->gl_profile, candidate->gl_doublebuffer);
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
        // Report the granted context only after the whole owner is healthy.
        std::printf("SDL GL config %d.%d profile=%d doublebuffer=%d\n",
                    candidate->gl_major, candidate->gl_minor,
                    candidate->gl_profile, candidate->gl_doublebuffer);
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

void* platform_gl_get_proc(const char* name)
{
    if (!active || !name || !*name ||
        SDL_GL_GetCurrentContext() != active->context.get() ||
        SDL_GL_GetCurrentWindow() != active->window.get()) return nullptr;
    return SDL_GL_GetProcAddress(name);
}

DrawableSize platform_drawable_size()
{
    if (!active || active->should_close ||
        (SDL_GetWindowFlags(active->window.get()) & SDL_WINDOW_MINIMIZED)) return {};
    DrawableSize size;
    SDL_GL_GetDrawableSize(active->window.get(), &size.width, &size.height);
    return size;
}

SwapIntervalReport platform_set_swap_interval(int requested)
{
    if ((requested != 0 && requested != 1) || !active || active->should_close ||
        SDL_GL_GetCurrentContext() != active->context.get() ||
        SDL_GL_GetCurrentWindow() != active->window.get()) return {};
    SwapIntervalReport report;
    report.attempted = true;
    report.accepted = SDL_GL_SetSwapInterval(requested) == 0;
    report.reported_interval = SDL_GL_GetSwapInterval();
    return report;
}

bool platform_present()
{
    if (!active || SDL_GL_GetCurrentContext() != active->context.get() ||
        SDL_GL_GetCurrentWindow() != active->window.get()) return false;
    const auto size = platform_drawable_size();
    if (size.width <= 0 || size.height <= 0) return false;
    SDL_GL_SwapWindow(active->window.get());
    return true;
}

WindowSize platform_window_size()
{
    if (!active || active->should_close ||
        (SDL_GetWindowFlags(active->window.get()) & SDL_WINDOW_MINIMIZED)) return {};
    WindowSize size;
    SDL_GetWindowSize(active->window.get(), &size.width, &size.height);
    return size;
}
WindowMouse platform_window_mouse()
{
    if (!active || active->should_close ||
        (SDL_GetWindowFlags(active->window.get()) & SDL_WINDOW_MINIMIZED) ||
        SDL_GetMouseFocus() != active->window.get()) return {};
    WindowMouse mouse;
    SDL_GetMouseState(&mouse.x, &mouse.y);
    mouse.available = true;
    return mouse;
}
