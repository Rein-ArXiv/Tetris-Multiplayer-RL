#include "renderer/handle_pool.h"
#ifndef POOL_NAMESPACE
#define POOL_NAMESPACE study_handles
#endif
namespace pool=POOL_NAMESPACE;
static pool::HandlePool<int> other;
pool::Handle other_handle(){return other.insert(std::make_unique<int>(42));}
bool other_accepts(pool::Handle h){return other.find(h)!=nullptr;}
