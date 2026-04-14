#pragma once
// platform/gl_defs.h — GL 2.0+ 타입 및 상수 (GL/gl.h 가 제공하지 않는 것만)
//
// win32.cpp 와 renderer.cpp 양쪽에서 include.
// GL/gl.h (OpenGL 1.1)가 이미 정의하는 GLenum, GLuint 등은 재정의하지 않음.

#include <cstddef>   // ptrdiff_t

typedef char      GLchar;      // GL 2.0 셰이더 소스 문자열용
typedef ptrdiff_t GLsizeiptr;  // GL 1.5 VBO 크기/오프셋용
typedef ptrdiff_t GLintptr;

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER           0x8892
#define GL_DYNAMIC_DRAW           0x88E8
#define GL_VERTEX_SHADER          0x8B31
#define GL_FRAGMENT_SHADER        0x8B30
#define GL_COMPILE_STATUS         0x8B81
#define GL_LINK_STATUS            0x8B82
#define GL_TEXTURE0               0x84C0
#define GL_CLAMP_TO_EDGE          0x812F
#define GL_RED                    0x1903
#endif
