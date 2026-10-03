#include <cstdio>
#include <cstdint>
#include <utility>
#include <cmath>

#include "SDL.h"
#include "audio/load_clip.h"
#include "audio/player.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <mp3file>\n", argv[0]);
        return 2;
    }

    study_audio::ClipResult loaded = study_audio::load_clip(argv[1]);
    if (!loaded.pcm.has_value()) {
        std::fprintf(stderr, "error: could not load '%s'\n", argv[1]);
        return 1;
    }

    const auto wait_ms = static_cast<Uint32>(std::ceil(loaded.pcm->layout().seconds * 1000.0)) + 2000u;
    study_audio::Player player;
    if (!player.open(loaded.pcm->rate(), loaded.pcm->channels())) {
        std::fprintf(stderr, "error: could not open audio device\n");
        return 1;
    }
    if (!player.replace(std::move(*loaded.pcm))) {
        std::fprintf(stderr, "error: could not stage clip\n");
        player.close();
        return 1;
    }
    if (!player.play()) {
        std::fprintf(stderr, "error: could not start playback\n");
        player.close();
        return 1;
    }

    const Uint32 start = SDL_GetTicks();
    bool finished = false;
    for (;;) {
        if (player.state() == study_audio::Player::State::finished) {
            finished = true;
            break;
        }
        if (static_cast<Uint32>(SDL_GetTicks() - start) >= wait_ms) break;
        SDL_Delay(5);
    }

    if (!finished) {
        player.stop();
        player.unload();
        player.close();
        return 3;
    }

    std::printf("submitted frames=%zu\n", player.cursor_frames());
    player.stop();
    player.unload();
    player.close();
    return 0;
}
