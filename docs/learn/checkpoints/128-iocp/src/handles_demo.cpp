#include "renderer/handle_pool.h"
#include <cstdio>
int main(){study_handles::HandlePool<int> images;
    const auto first=images.insert(std::make_unique<int>(10));
    const auto borrowed=first;
    images.erase(first);
    const auto replacement=images.insert(std::make_unique<int>(20));
    std::printf("same slot=%d, same token=%d, old found=%d, new value=%d\n",
        unsigned(first)==unsigned(replacement),first==replacement,images.find(borrowed)!=nullptr,*images.find(replacement));
}
