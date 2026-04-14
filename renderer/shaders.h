#pragma once

// ─────────────────────────────────────────────────────────────────────────────
// renderer/shaders.h — GLSL 셰이더 소스 문자열
//
// 학습 포인트:
//   GPU는 삼각형을 처리할 때 두 가지 셰이더 프로그램을 반드시 실행합니다.
//
//   Vertex Shader:  각 꼭짓점(vertex)에 대해 실행.
//                   "이 꼭짓점은 화면의 어느 위치에 있는가?"를 계산.
//                   입력: 오브젝트 좌표(pos), 변환 행렬(u_proj)
//                   출력: gl_Position (NDC 좌표, -1~1 범위)
//
//   Fragment Shader: 삼각형 내부의 각 픽셀에 대해 실행.
//                    "이 픽셀의 색은 무엇인가?"를 계산.
//
//   우리 셰이더는 최소한으로 설계:
//   - 색칠된 사각형: u_color uniform 하나로 전체 사각형이 같은 색
//   - 텍스트: 텍스처에서 알파값을 읽어 u_color에 곱함 (문자 모양 마스크)
//
//   직교 투영(Orthographic Projection):
//   화면 좌표 (0,0)~(720,640)을 NDC (-1,-1)~(+1,+1)로 변환하는 행렬.
//   u_proj에 매 프레임 업로드. 이걸로 "픽셀 좌표를 그냥 쓸 수 있게" 된다.
// ─────────────────────────────────────────────────────────────────────────────

// ─── 색칠된 사각형용 셰이더 ───────────────────────────────────────────────────
static const char* kRectVert = R"glsl(
#version 130
in vec2 a_pos;
uniform mat4 u_proj;
void main() {
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
}
)glsl";

static const char* kRectFrag = R"glsl(
#version 130
uniform vec4 u_color;
out vec4 fragColor;
void main() {
    fragColor = u_color;
}
)glsl";

// ─── 텍스트(폰트 텍스처)용 셰이더 ────────────────────────────────────────────
// a_uv: 텍스처 좌표 (0~1 범위, 폰트 아틀라스 내 위치)
// u_tex: 폰트 아틀라스 텍스처 (각 채널의 R값이 알파 마스크)
// 결과: u_color 에 텍스처 알파를 곱해 문자 모양만 색칠
static const char* kTextVert = R"glsl(
#version 130
in vec2 a_pos;
in vec2 a_uv;
out vec2 v_uv;
uniform mat4 u_proj;
void main() {
    v_uv = a_uv;
    gl_Position = u_proj * vec4(a_pos, 0.0, 1.0);
}
)glsl";

static const char* kTextFrag = R"glsl(
#version 130
in vec2 v_uv;
out vec4 fragColor;
uniform sampler2D u_tex;
uniform vec4 u_color;
void main() {
    float alpha = texture(u_tex, v_uv).r;
    fragColor = vec4(u_color.rgb, u_color.a * alpha);
}
)glsl";
