#include "audio/mixer.h"

#include <cstdint>
#include <cstdio>

int main() {
    using study_audio::Mixer;
    using study_audio::Pcm16;

    auto a = Pcm16::make(44100, 1, {30000});
    auto b = Pcm16::make(44100, 1, {30000});
    auto c = Pcm16::make(44100, 1, {-30000});
    if (!a || !b || !c) return 1;

    Mixer mixer;
    if (!mixer.configure(44100, 1)) return 1;
    if (!mixer.start(0, *a)) return 1;
    if (!mixer.start(1, *b)) return 1;
    if (!mixer.start(2, *c)) return 1;

    std::int16_t mixed = 0;
    mixer.render(&mixed, sizeof(mixed));

    // Legacy policy: clamp after every addition.
    std::int32_t legacy = 0;
    const std::int32_t raw[3] = {30000, 30000, -30000};
    for (std::int32_t s : raw) {
        legacy += s;
        if (legacy < -32768) legacy = -32768;
        else if (legacy > 32767) legacy = 32767;
    }

    const std::int32_t sum = 30000 + 30000 - 30000;
    std::printf("sum=%d mixed=%d legacy=%d\n", static_cast<int>(sum),
                static_cast<int>(mixed), static_cast<int>(legacy));

    return (mixed == 30000 && legacy == 2767) ? 0 : 1;
}
