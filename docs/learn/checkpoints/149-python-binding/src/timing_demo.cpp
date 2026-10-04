#include "renderer/cpu_timing.h"
#include <cstdio>
#include <limits>
int main(){
    const auto max=std::numeric_limits<std::uint64_t>::max();
    const auto result=study_timing::summarize(max-6,max-5,max-3,max,1000);
    if(!result)return 1;
    std::printf("Synthetic CPU stages: submit=%.1fms sync=%.1fms present=%.1fms\n",
                result->submit_ms,result->sync_ms,result->present_ms);
    std::puts("These are chosen example timestamps, not measured GPU/display times.");
}
