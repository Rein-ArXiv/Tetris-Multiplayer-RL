#pragma once
#include "renderer/piece_scene.h"
namespace study_ghost_view {
// Opaque hint: no blend state dependency. Draw before the active piece.
inline constexpr char fragment[] = R"glsl(#version 330 core
layout(location = 0) out vec4 out_color;
void main() { out_color = vec4(0.5,0.5,0.5,1); }
)glsl";
} // namespace study_ghost_view
