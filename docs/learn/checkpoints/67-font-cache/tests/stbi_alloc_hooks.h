#pragma once
#include <cstdlib>
#include <cstddef>
inline std::size_t stbi_blocks=0;
inline void* tracked_malloc(std::size_t n){auto* p=std::malloc(n);if(p)++stbi_blocks;return p;}
inline void tracked_free(void* p){if(p){--stbi_blocks;std::free(p);}}
inline void* tracked_realloc(void* p,std::size_t n){if(n==0){tracked_free(p);return nullptr;}const bool was_null=p==nullptr;void* out=std::realloc(p,n);if(out&&was_null)++stbi_blocks;return out;}
#define STBI_MALLOC(n) tracked_malloc(n)
#define STBI_FREE(p) tracked_free(p)
#define STBI_REALLOC(p,n) tracked_realloc(p,n)
