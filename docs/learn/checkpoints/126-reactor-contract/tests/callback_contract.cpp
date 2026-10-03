// tests/callback_contract.cpp
//
// Diagnostic contract for the ACTUAL Player implementation. This translation
// unit textually includes audio/player.cpp (override the path with
// -DSTUDY_PLAYER_IMPL="...") after routing the four SDL audio entry points
// through deterministic wrappers, then drives Player::callback BY HAND so the
// real SDL worker can never overlap. It does not re-test SDL scheduling; the
// existing playback_contract owns that.
//
// Built by CMake; study_pcm is an INTERFACE target, not an archive.

// ---- SDL headers first --------------------------------------------------
#include "SDL.h"

// ---- runtime headers ----------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef STUDY_PLAYER_IMPL
#define STUDY_PLAYER_IMPL "audio/player.cpp"
#endif

// ------------------------------------------------------------------------
// Instrumentation. Declared before the allocation replacements and the SDL
// wrappers, which both consult it.
// ------------------------------------------------------------------------
namespace study_test {

// Nesting depth of the SDL device lock, maintained by the wrapper pair.
thread_local int lock_depth = 0;

// True only around a deterministic, hand-driven callback invocation.
thread_local bool callback_region = false;

// C++ (operator new/delete) accounting. The overrides below never do I/O.
thread_local unsigned long long cpp_alloc_total = 0;
thread_local unsigned long long cpp_free_total = 0;
thread_local unsigned long long cpp_alloc_guarded = 0;
thread_local unsigned long long cpp_free_guarded = 0;

inline bool guarded() noexcept { return callback_region || lock_depth > 0; }

inline void reset_counters() noexcept {
    cpp_alloc_total = 0;
    cpp_free_total = 0;
    cpp_alloc_guarded = 0;
    cpp_free_guarded = 0;
}

// Captured straight out of the SDL_AudioSpec Player::open hands to realOpen.
SDL_AudioCallback captured_callback = nullptr;
void* captured_userdata = nullptr;
int pause_calls = 0;

}  // namespace study_test

// ------------------------------------------------------------------------
// Replaced global allocation functions. They count C++ allocations only;
// SDL/C allocations made with malloc() are intentionally NOT tracked.
// ------------------------------------------------------------------------
// Keep diagnostic allocation boundaries visible to the optimizer. Inlining
// a malloc-backed replacement new into a vector constructor can also trigger
// a spurious mismatched-new-delete diagnostic in optimized GCC builds.
#if defined(__GNUC__) || defined(__clang__)
#define STUDY_DIAGNOSTIC_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define STUDY_DIAGNOSTIC_NOINLINE __declspec(noinline)
#else
#define STUDY_DIAGNOSTIC_NOINLINE
#endif
STUDY_DIAGNOSTIC_NOINLINE void* operator new(std::size_t n) {
    ++study_test::cpp_alloc_total;
    if (study_test::guarded()) ++study_test::cpp_alloc_guarded;
    void* p = std::malloc(n == 0 ? 1 : n);
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

STUDY_DIAGNOSTIC_NOINLINE void operator delete(void* p) noexcept {
    if (p != nullptr) {
        ++study_test::cpp_free_total;
        if (study_test::guarded()) ++study_test::cpp_free_guarded;
    }
    std::free(p);
}

STUDY_DIAGNOSTIC_NOINLINE void operator delete(void* p, std::size_t) noexcept {
    if (p != nullptr) {
        ++study_test::cpp_free_total;
        if (study_test::guarded()) ++study_test::cpp_free_guarded;
    }
    std::free(p);
}

// ------------------------------------------------------------------------
// Deterministic SDL wrappers. Defined BEFORE the renaming macros so their
// bodies call the genuine SDL functions.
// ------------------------------------------------------------------------
namespace study_test {

SDL_AudioDeviceID wrapper_OpenAudioDevice(const char* name, int iscapture,
                                          const SDL_AudioSpec* desired,
                                          SDL_AudioSpec* obtained,
                                          int allowed_changes) {
    if (desired != nullptr) {
        captured_callback = desired->callback;
        captured_userdata = desired->userdata;
    }
    return SDL_OpenAudioDevice(name, iscapture, desired, obtained,
                               allowed_changes);
}

void wrapper_PauseAudioDevice(SDL_AudioDeviceID dev, int pause_on) {
    (void)pause_on;
    // Force pause even for pause_on == 0: the real device must stay paused so
    // no SDL worker thread can race the manual callback invocations.
    SDL_PauseAudioDevice(dev, 1);
    ++pause_calls;
}

void wrapper_LockAudioDevice(SDL_AudioDeviceID dev) {
    SDL_LockAudioDevice(dev);
    ++lock_depth;
}

void wrapper_UnlockAudioDevice(SDL_AudioDeviceID dev) {
    --lock_depth;
    SDL_UnlockAudioDevice(dev);
}

}  // namespace study_test

// Route the implementation's SDL calls through the wrappers, include the real
// translation unit, then restore the names.
#define SDL_OpenAudioDevice   study_test::wrapper_OpenAudioDevice
#define SDL_PauseAudioDevice  study_test::wrapper_PauseAudioDevice
#define SDL_LockAudioDevice   study_test::wrapper_LockAudioDevice
#define SDL_UnlockAudioDevice study_test::wrapper_UnlockAudioDevice

#include STUDY_PLAYER_IMPL

#undef SDL_OpenAudioDevice
#undef SDL_PauseAudioDevice
#undef SDL_LockAudioDevice
#undef SDL_UnlockAudioDevice

// The retirement path needs these Pcm16 guarantees.
static_assert(std::is_nothrow_move_constructible<study_audio::Pcm16>::value,
              "Pcm16 move construction must be noexcept");
static_assert(std::is_nothrow_move_assignable<study_audio::Pcm16>::value,
              "Pcm16 move assignment must be noexcept");

// ------------------------------------------------------------------------
// Independent reference signal.
// ------------------------------------------------------------------------
namespace {

constexpr std::uint32_t kRate = 44100;
constexpr std::uint32_t kChannels = 2;
constexpr std::size_t kFrames = 44100;
constexpr std::size_t kFrameBytes = kChannels * 2;  // S16
constexpr std::size_t kToneBytes = kFrames * kFrameBytes;

}  // namespace

static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__,         \
                         __LINE__, #cond);                                     \
            std::exit(1);                                                      \
        }                                                                      \
    } while (0)

int main() {
    study_audio::Player p;

    CHECK(p.state() == study_audio::Player::State::closed);

    CHECK(p.open(kRate, kChannels));
    CHECK(study_test::captured_callback != nullptr);
    CHECK(study_test::captured_userdata != nullptr);
    CHECK(p.state() == study_audio::Player::State::empty);

    // Independent reference tone, installed before any counter snapshot.
    CHECK(p.replace(study_audio::make_reference_tone()));
    CHECK(p.state() == study_audio::Player::State::ready);

    CHECK(p.play());
    CHECK(study_test::pause_calls >= 1);  // real device stayed paused
    CHECK(p.state() == study_audio::Player::State::playing);

    // Independent reference bytes for the exact expected output, materialised
    // before the allocation counters are snapshotted for the callback region.
    std::vector<unsigned char> reference(kToneBytes);
    const auto reference_pcm = study_audio::make_reference_tone();
    std::memcpy(reference.data(), reference_pcm.samples().data(), kToneBytes);

    // ---- hand-driven callback: 90 calls, 512 whole frames each ----------
    study_test::reset_counters();
    std::size_t byte_mismatches = 0;
    for (int call = 0; call < 90; ++call) {
        unsigned char frame[2051];
        std::memset(frame, 0xCC, sizeof frame);

        study_test::callback_region = true;
        study_test::captured_callback(study_test::captured_userdata, frame + 1,
                                      2049);
        study_test::callback_region = false;

        // Checks run OUTSIDE the callback region.
        CHECK(frame[0] == 0xCC);     // leading guard
        CHECK(frame[2050] == 0xCC);  // trailing guard
        CHECK(frame[2049] == 0x00);  // zero byte for the partial frame

        const std::size_t base = static_cast<std::size_t>(call) * 2048;
        for (std::size_t b = 0; b < 2048; ++b) {
            const std::size_t g = base + b;
            const unsigned char want =
                (g < kToneBytes) ? reference[g] : static_cast<unsigned char>(0);
            if (frame[1 + b] != want) ++byte_mismatches;
        }
    }
    CHECK(byte_mismatches == 0);
    CHECK(study_test::cpp_alloc_guarded == 0);
    CHECK(study_test::cpp_free_guarded == 0);
    CHECK(p.state() == study_audio::Player::State::finished);
    CHECK(p.cursor_frames() == kFrames);

    // ---- len <= 0 must not touch userdata or the output buffer ----------
    unsigned char poison[4];
    std::memset(poison, 0xCC, sizeof poison);
    study_test::captured_callback(nullptr, poison, 0);
    study_test::captured_callback(nullptr, poison, -1);
    CHECK(poison[0] == 0xCC && poison[1] == 0xCC && poison[2] == 0xCC &&
          poison[3] == 0xCC);

    // ---- replace retires the old PCM after unlock -----------------------
    auto replacement = study_audio::make_reference_tone();
    study_test::reset_counters();
    CHECK(p.replace(std::move(replacement)));
    CHECK(study_test::cpp_alloc_guarded == 0);
    CHECK(study_test::cpp_free_total > 0);  // old PCM was deallocated
#ifdef LEGACY_EXPECT_LOCKED_FREE
    // The immutable 76-playback implementation: the retired buffer was freed under the device lock.
    CHECK(study_test::cpp_free_guarded > 0);
#else
    CHECK(study_test::cpp_free_guarded == 0);  // ... and never under the lock
#endif
    CHECK(p.state() == study_audio::Player::State::ready);

    // ---- unload retires the old PCM after unlock ------------------------
    study_test::reset_counters();
    p.unload();
    CHECK(study_test::cpp_alloc_guarded == 0);
    CHECK(study_test::cpp_free_total > 0);
#ifdef LEGACY_EXPECT_LOCKED_FREE
    CHECK(study_test::cpp_free_guarded > 0);
#else
    CHECK(study_test::cpp_free_guarded == 0);
#endif
    CHECK(p.state() == study_audio::Player::State::empty);

    CHECK(!p.play());
    CHECK(study_test::lock_depth == 0);
    // Independent slot lifetimes: retiring slot zero must preserve slot one.
    auto constant=[](int value){return study_audio::Pcm16::make(kRate,kChannels,
        std::vector<std::int16_t>(64,static_cast<std::int16_t>(value)));};
    auto left=constant(1000),right=constant(2000);CHECK(left&&right);
    CHECK(p.replace(std::move(*left),0)&&p.replace(std::move(*right),1));
    CHECK(p.play(0)&&p.play(1));
    auto sample=[&](){
        unsigned char out[4]{};
        study_test::captured_callback(study_test::captured_userdata,out,4);
        std::int16_t a=0,b=0;std::memcpy(&a,out,2);std::memcpy(&b,out+2,2);
        CHECK(a==b);return a;
    };
    CHECK(sample()==3000);
    CHECK(p.cursor_frames(0)==1&&p.cursor_frames(1)==1);
    auto rejected=study_audio::Pcm16::make(48000,2,std::vector<std::int16_t>(8,42));
    CHECK(rejected&&!p.replace(std::move(*rejected),1));
    CHECK(sample()==3000&&p.cursor_frames(1)==2);
    auto new_left=constant(300);CHECK(new_left);
    study_test::reset_counters();
    CHECK(p.replace(std::move(*new_left),0));
    CHECK(study_test::cpp_free_guarded==0&&study_test::cpp_free_total>0);
    CHECK(p.play(0));CHECK(sample()==2300);
    const auto paused=study_test::pause_calls;
    study_test::reset_counters();p.unload(0);
    CHECK(study_test::cpp_free_guarded==0&&study_test::cpp_free_total>0);
    CHECK(study_test::pause_calls==paused);
    CHECK(sample()==2000&&p.cursor_frames(1)==4);
    CHECK(!p.play(audio_mix::kMaxVoices));
    CHECK(p.state(audio_mix::kMaxVoices)==study_audio::Player::State::empty);
    CHECK(p.cursor_frames(audio_mix::kMaxVoices)==0);
    p.unload(audio_mix::kMaxVoices);
    CHECK(p.play(1));CHECK(p.cursor_frames(1)==0); // new voice has its own cursor
    CHECK(sample()==4000); // same PCM, two independent voices
    study_test::reset_counters();p.unload(1); // both voices must detach before PCM free
    CHECK(study_test::cpp_free_guarded==0&&study_test::cpp_free_total>0);
    CHECK(sample()==0&&p.state(1)==study_audio::Player::State::empty);
    p.stop();CHECK(sample()==0);
    p.close();
    CHECK(p.state() == study_audio::Player::State::closed);

    if (g_failures != 0) {
        std::fprintf(stderr, "callback_contract: %d check(s) failed\n",
                     g_failures);
        return 1;
    }
    std::fprintf(stderr, "callback_contract: all checks passed\n");
    return 0;
}
