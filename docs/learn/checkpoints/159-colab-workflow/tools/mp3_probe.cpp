#include "audio/load_clip.h"
#include <iostream>
int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: mp3_probe path.mp3\n"; return 2; }
    const auto loaded = study_audio::load_clip(argv[1]);
    if (!loaded) {
        std::cerr << "load failed: " << audio_mp3::error_name(loaded.error) << '\n';
        return 1;
    }
    const auto& pcm = *loaded.pcm;
    const auto layout = pcm.layout();
    std::cout << "rate=" << pcm.rate() << " channels=" << pcm.channels()
              << " frames=" << layout.frames << " samples=" << layout.samples
              << " bytes=" << layout.bytes << " seconds=" << layout.seconds << '\n';
}
