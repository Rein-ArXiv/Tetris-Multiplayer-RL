#pragma once

// GLSL 330 core 셰이더. 이 렌더러는 프로그램을 **하나만** 쓴다.
//
// 도형마다 셰이더를 나누면 그릴 때마다 glUseProgram 이 끼어들어 배칭이 끊긴다.
// 사각형 · 둥근 사각형 · 텍스트 글리프 · 이미지가 전부 "텍스처를 입힌 사각형"
// 으로 표현되도록 정점 형식을 맞추고, 차이는 정점 속성으로 넘긴다.
//
//   · 단색 도형    → 1x1 흰 텍스처를 샘플링 (항상 1.0 이므로 색이 그대로)
//   · 이미지       → 해당 텍스처, a_color 는 tint
//   · 글리프       → R8 텍스처의 r 채널을 알파로 사용 (a_channel = 1)
//   · 둥근 모서리  → a_half / a_radius 로 fragment 에서 SDF 계산
//
// 덕분에 프레임 전체가 텍스처 교체 지점에서만 나뉜다.

static const char* kQuadVert = R"glsl(
#version 330 core

layout(location = 0) in vec2  a_pos;      // 화면 픽셀 좌표 (좌상단 원점)
layout(location = 1) in vec2  a_uv;
layout(location = 2) in vec4  a_color;
layout(location = 3) in vec2  a_local;    // 사각형 중심 기준 좌표 (픽셀)
layout(location = 4) in vec2  a_half;     // 사각형 반크기 (픽셀)
layout(location = 5) in float a_radius;   // 모서리 반지름 (0 이면 각진 사각형)
layout(location = 6) in float a_channel;  // 0 = RGBA 텍스처, 1 = R8 을 알파로

uniform vec2 u_screen;                    // 논리 해상도 (픽셀)

out vec2  v_uv;
out vec4  v_color;
out vec2  v_local;
out vec2  v_half;
out float v_radius;
out float v_channel;

void main() {
    // 픽셀 좌표 → NDC. y 는 화면이 아래로 증가하므로 뒤집는다.
    vec2 ndc = vec2( 2.0 * a_pos.x / u_screen.x - 1.0,
                     1.0 - 2.0 * a_pos.y / u_screen.y );
    gl_Position = vec4(ndc, 0.0, 1.0);

    v_uv      = a_uv;
    v_color   = a_color;
    v_local   = a_local;
    v_half    = a_half;
    v_radius  = a_radius;
    v_channel = a_channel;
}
)glsl";

static const char* kQuadFrag = R"glsl(
#version 330 core

in vec2  v_uv;
in vec4  v_color;
in vec2  v_local;
in vec2  v_half;
in float v_radius;
in float v_channel;

uniform sampler2D u_tex;

out vec4 fragColor;

// 둥근 사각형의 signed distance. 음수면 안쪽, 양수면 바깥쪽.
// p 는 중심 기준 좌표, b 는 반크기, r 은 모서리 반지름.
float rounded_box_sdf(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + vec2(r);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

void main() {
    vec4 tex = texture(u_tex, v_uv);

    // R8 글리프는 r 채널이 coverage 다. RGBA 이미지는 그대로 쓴다.
    vec4 sampled = mix(tex, vec4(1.0, 1.0, 1.0, tex.r), v_channel);

    vec4 c = sampled * v_color;

    // 반지름이 0 이면 SDF 를 건너뛴다. 각진 사각형에서 경계 픽셀이
    // 불필요하게 흐려지는 것을 막는다.
    if (v_radius > 0.0) {
        float d = rounded_box_sdf(v_local, v_half, v_radius);
        // 1픽셀 폭으로 부드럽게 자른다 — 모서리 안티앨리어싱이
        // 별도 코드 없이 따라온다.
        c.a *= 1.0 - smoothstep(-0.5, 0.5, d);
    }

    if (c.a <= 0.0) discard;
    fragColor = c;
}
)glsl";
