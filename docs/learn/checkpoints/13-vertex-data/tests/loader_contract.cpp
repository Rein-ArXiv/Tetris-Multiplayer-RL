#include "renderer/gl_api.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned missing_mask, lookup_count, call_count;
static const unsigned char* STUDY_GL_CALL get_string(study_gl::GLenum) {
    ++call_count;
    return reinterpret_cast<const unsigned char*>("test driver");
}
static void STUDY_GL_CALL get_integer(study_gl::GLenum,study_gl::GLint* value) {
    ++call_count; *value = 16;
}
static study_gl::GLenum STUDY_GL_CALL get_error() { ++call_count; return 0; }
static void require(bool value,const char* message) {
    if (!value) { std::fprintf(stderr,"loader contract: %s\n",message); std::exit(1); }
}
static void* resolve(const char* name) {
    const char* names[] = {"glGetString","glGetIntegerv","glGetError"};
    require(lookup_count < 3 && std::strcmp(name,names[lookup_count]) == 0,"lookup order/name");
    unsigned index = lookup_count++;
    if (missing_mask & (1u << index)) return nullptr;
    if (index == 0) return reinterpret_cast<void*>(get_string);
    if (index == 1) return reinterpret_cast<void*>(get_integer);
    return reinterpret_cast<void*>(get_error);
}
int main() {
    study_gl::GlApi api;
    for (unsigned mask=0;mask<8;++mask) {
        // Seed with stale pointers: a failed reload must clear them all.
        api.GetString = get_string; api.GetIntegerv = get_integer; api.GetError = get_error;
        missing_mask = mask; lookup_count = call_count = 0;
        require(study_gl::load(api,resolve) == (mask == 0),"result");
        require(lookup_count == 3 && call_count == 0,"resolve all, invoke none");
        if (mask) require(!api.GetString && !api.GetIntegerv && !api.GetError,"failed load cleared table");
        else {
            study_gl::GLint value = 0;
            require(api.GetString(study_gl::Version) != nullptr,"string call");
            api.GetIntegerv(study_gl::MaxVertexAttribs,&value);
            require(value == 16 && api.GetError() == 0 && call_count == 3,"typed calls");
        }
    }
    api.GetString = get_string;
    require(!study_gl::load(api,nullptr) && !api.GetString && !api.GetIntegerv && !api.GetError,"null resolver");
    std::puts("Loader: 8 missing-symbol combinations, no early calls, cleared reload and null resolver passed");
}
