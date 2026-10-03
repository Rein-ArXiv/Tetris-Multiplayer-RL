#pragma once
#include "../renderer/image.h"
#include <vector>

// Client-only styling. Never read by SimGame, relay, replay, or Python bindings.
// Load once after renderer_init, before creating any Game instances.
void presentation_load(const char* path);
std::vector<Color> presentation_palette(std::vector<Color> defaults);
void presentation_draw_avatar(ImageHandle image, int x, int y, int size,
                              bool opponent, double seconds, bool animate);
// Fit the whole illustration within a fixed integer rectangle. Reject invalid
// geometry and omit images whose thin side would round down to zero.
void presentation_draw_portrait(ImageHandle image, int x, int y, int w, int h);
