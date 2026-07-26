#pragma once
#include <cstddef>
#include <cstdint>

// OpenGL 3.3 Core 중 이 렌더러가 실제로 쓰는 것만 선언한다.
//
// 왜 직접 선언하는가: Windows 의 opengl32.dll 은 GL 1.1 까지만 export 한다.
// 3.3 의 셰이더·VAO·VBO 함수는 링커가 찾을 수 없고, 런타임에 드라이버에서
// 주소를 받아야 한다. Linux/macOS 는 libGL 에 심볼이 있지만 플랫폼마다
// 다른 코드를 쓰지 않으려고 세 곳 모두 같은 조회 경로를 탄다.
//
// glad/GLEW 를 쓰지 않는 이유는 의존성 하나를 아끼려는 것이 아니라,
// "GL 함수가 어디서 오는가" 가 이 프로젝트에서 감출 이유가 없는 지식이기
// 때문이다. 필요한 함수가 40개 남짓이라 직접 들고 있어도 부담이 없다.

using GLenum     = unsigned int;
using GLbitfield = unsigned int;
using GLuint     = unsigned int;
using GLint      = int;
using GLsizei    = int;
using GLfloat    = float;
using GLboolean  = unsigned char;
using GLchar     = char;
using GLintptr   = std::ptrdiff_t;
using GLsizeiptr = std::ptrdiff_t;

#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_TRIANGLES                      0x0004
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_FLOAT                          0x1406
#define GL_RGBA                           0x1908
#define GL_RED                            0x1903
#define GL_R8                             0x8229
#define GL_RGBA8                          0x8058
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_BLEND                          0x0BE2
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_ONE                            1
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_ARRAY_BUFFER                   0x8892
#define GL_STREAM_DRAW                    0x88E0
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_SCISSOR_TEST                   0x0C11
#define GL_MAX_TEXTURE_SIZE               0x0D33
#define GL_VERSION                        0x1F02
#define GL_RENDERER                       0x1F01
#define GL_NO_ERROR                       0

// ─── 함수 포인터 ──────────────────────────────────────────────────────────────
// 이름 앞에 gl_ 을 붙여 시스템 헤더의 gl* 심볼과 충돌하지 않게 한다.

#define GL_FUNCS(X)                                                            \
    X(void,   Enable,                 (GLenum))                                \
    X(void,   Disable,                (GLenum))                                \
    X(void,   BlendFunc,              (GLenum, GLenum))                        \
    X(void,   Viewport,               (GLint, GLint, GLsizei, GLsizei))        \
    X(void,   Scissor,                (GLint, GLint, GLsizei, GLsizei))        \
    X(void,   ClearColor,             (GLfloat, GLfloat, GLfloat, GLfloat))    \
    X(void,   Clear,                  (GLbitfield))                            \
    X(void,   DrawArrays,             (GLenum, GLint, GLsizei))                \
    X(GLenum, GetError,               (void))                                  \
    X(const unsigned char*, GetString,(GLenum))                                \
    X(void,   GetIntegerv,            (GLenum, GLint*))                         \
    X(void,   PixelStorei,            (GLenum, GLint))                         \
    X(GLuint, CreateShader,           (GLenum))                                \
    X(void,   ShaderSource,           (GLuint, GLsizei, const GLchar* const*, const GLint*)) \
    X(void,   CompileShader,          (GLuint))                                \
    X(void,   GetShaderiv,            (GLuint, GLenum, GLint*))                \
    X(void,   GetShaderInfoLog,       (GLuint, GLsizei, GLsizei*, GLchar*))    \
    X(void,   DeleteShader,           (GLuint))                                \
    X(GLuint, CreateProgram,          (void))                                  \
    X(void,   AttachShader,           (GLuint, GLuint))                        \
    X(void,   LinkProgram,            (GLuint))                                \
    X(void,   GetProgramiv,           (GLuint, GLenum, GLint*))                \
    X(void,   GetProgramInfoLog,      (GLuint, GLsizei, GLsizei*, GLchar*))    \
    X(void,   UseProgram,             (GLuint))                                \
    X(void,   DeleteProgram,          (GLuint))                                \
    X(GLint,  GetUniformLocation,     (GLuint, const GLchar*))                 \
    X(void,   Uniform1i,              (GLint, GLint))                          \
    X(void,   Uniform2f,              (GLint, GLfloat, GLfloat))               \
    X(void,   GenVertexArrays,        (GLsizei, GLuint*))                      \
    X(void,   BindVertexArray,        (GLuint))                                \
    X(void,   DeleteVertexArrays,     (GLsizei, const GLuint*))                \
    X(void,   GenBuffers,             (GLsizei, GLuint*))                      \
    X(void,   BindBuffer,             (GLenum, GLuint))                        \
    X(void,   BufferData,             (GLenum, GLsizeiptr, const void*, GLenum)) \
    X(void,   DeleteBuffers,          (GLsizei, const GLuint*))                \
    X(void,   VertexAttribPointer,    (GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)) \
    X(void,   EnableVertexAttribArray,(GLuint))                                \
    X(void,   GenTextures,            (GLsizei, GLuint*))                      \
    X(void,   BindTexture,            (GLenum, GLuint))                        \
    X(void,   ActiveTexture,          (GLenum))                                \
    X(void,   TexImage2D,             (GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*)) \
    X(void,   TexSubImage2D,          (GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*)) \
    X(void,   TexParameteri,          (GLenum, GLenum, GLint))                 \
    X(void,   DeleteTextures,         (GLsizei, const GLuint*))

#define GL_DECLARE(ret, name, args) extern ret (*gl_##name) args;
GL_FUNCS(GL_DECLARE)
#undef GL_DECLARE

// 모든 함수 포인터를 채운다. 하나라도 못 받으면 false 와 함께 그 이름을 찍는다.
// platform_gl_get_proc 을 통해 조회하므로 컨텍스트가 current 인 상태여야 한다.
bool gl_load_functions();
