#include "loop/frame_runner.h"
#include "src/spin_example.h"
#include <cstdio>
int main() {
    study_loop::FrameRunner runner(*study_round::Round::create(make_spin_board(),study_catalog::Kind::T));
    unsigned frame=0;
    for(double dt:{0.008,0.009,0.05}) {
        study_loop::FrameInput raw;if(frame==0)raw.up=raw.drop=true;
        const auto report=runner.advance(dt,raw);if(!report)return 1;
        unsigned locks=0;for(unsigned i=0;i<report->ticks;++i)locks+=bool(report->observations[i].lock);
        std::printf("frame=%u ticks=%u locks=%u boardChanged=%d score=%llu lastSpin=%d\n",++frame,
            report->ticks,locks,report->board_changed,(unsigned long long)runner.round().score(),runner.round().last_t_spin_lines());
    }
}
