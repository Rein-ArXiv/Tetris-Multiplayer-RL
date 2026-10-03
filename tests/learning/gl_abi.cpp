// Compare selected production signatures with installed Khronos core headers.
// On Linux this validates types, not native 32-bit Windows execution.
#include <GL/glcorearb.h>
#include "../../renderer/gl_api.h"
#include <type_traits>
static_assert(std::is_same_v<decltype(gl_GetString),PFNGLGETSTRINGPROC>);
static_assert(std::is_same_v<decltype(gl_ClearColor),PFNGLCLEARCOLORPROC>);
static_assert(std::is_same_v<decltype(gl_BufferData),PFNGLBUFFERDATAPROC>);
static_assert(std::is_same_v<decltype(gl_ShaderSource),PFNGLSHADERSOURCEPROC>);
int main() {}
