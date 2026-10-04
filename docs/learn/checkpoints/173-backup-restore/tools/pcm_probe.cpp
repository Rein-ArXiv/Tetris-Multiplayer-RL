// tools/pcm_probe.cpp - stdout probe for the prepared reference tone.
// No file export and no audio device: deterministic, testable stdout only.

#include "audio/pcm_s16.h"

#include <cstddef>
#include <iostream>

int main() {
    const study_audio::Pcm16 tone = study_audio::make_reference_tone();
    const audio_pcm::Layout& layout = tone.layout();

    std::cout << "rate=" << tone.rate()
              << " channels=" << tone.channels()
              << " frames=" << layout.frames
              << " samples=" << layout.samples
              << " bytes=" << layout.bytes
              << " seconds=" << layout.seconds << '\n';

    const std::size_t landmarks[] = {0, 25, 50, 75, 100};
    std::cout << "landmarks=";
    for (std::size_t i = 0; i < sizeof(landmarks) / sizeof(landmarks[0]); ++i) {
        if (i != 0) {
            std::cout << ',';
        }
        std::cout << tone.at(landmarks[i], 0);
    }
    std::cout << '\n';
    return 0;
}
