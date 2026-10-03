"""Allocation-failure and numeric boundary checks against current root/teaching headers."""
from pathlib import Path
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'out/learning-checkpoints/129-root-timers';CP=ROOT/'docs/learn/checkpoints/129-timers'
SOURCE=r'''
#include "HEADER"
#include <cstdlib>
#include <new>
#include <cstdio>
#include <stdexcept>
#include <limits>
static int allocations = -1;
void* operator new(std::size_t n) {
    if (allocations == 0) throw std::bad_alloc();
    if (allocations > 0) --allocations;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
using Queue = QUEUE;
using Token = TOKEN;
using T = Queue::TimePoint;
using D = Queue::Clock::duration;
using ms = std::chrono::milliseconds;
void require(bool v) { if (!v) throw std::runtime_error("timer root contract"); }
int main() {
 try {
    TOKENS
    for (int budget = 0; budget < 8; ++budget) {
        Queue q; q.arm(a, T{} + ms(10));
        bool failed = false; allocations = budget;
        try { q.arm(b, T{} + ms(1)); } catch (const std::bad_alloc&) { failed = true; }
        allocations = -1;
        std::vector<Token> due; q.expired(T{} + ms(20), due);
        require(failed ? due == std::vector<Token>{a} : due == std::vector<Token>({b, a}));
        require(q.empty());
    }
    for (int budget = 0; budget < 4; ++budget) {
        Queue q; q.arm(a, T{} + ms(10));
        bool failed = false; allocations = budget;
        try { q.arm(a, T{} + ms(1)); } catch (const std::bad_alloc&) { failed = true; }
        allocations = -1;
        require(q.timeout_ms(T{}) == (failed ? 10 : 1));
    }
    Queue q; q.arm(a, T{});
    std::vector<Token> due; bool failed = false; allocations = 0;
    try { q.expired(T{}, due); } catch (const std::bad_alloc&) { failed = true; }
    allocations = -1; require(failed && due.empty() && !q.empty());
    q.expired(T{}, due); require(due == std::vector<Token>{a} && q.empty());
    q.arm(a, T::max()); require(q.timeout_ms(T::min()) == 0x3fffffff); q.cancel(a);
    q.arm(a, T::min()); require(q.timeout_ms(T::max()) == 0); q.cancel(a);
    q.arm(a, T::max()); require(q.timeout_ms(T::max() - D(1)) == 1); q.cancel(a);
    // Independent wide-integer model avoids overflowing chrono subtraction.
    __extension__ using Wide = __int128;
    std::uint64_t state = 88172645463325252ULL;
    auto rng = [&] { state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; };
    for (int i = 0; i < 20000; ++i) {
        const auto n = static_cast<D::rep>(rng());
        const auto w = static_cast<D::rep>(rng());
        const Wide diff = static_cast<Wide>(w) - n;
        Wide expected = 0;
        if (diff > 0) {
            const Wide numerator = diff * D::period::num * 1000;
            expected = (numerator + D::period::den - 1) / D::period::den;
            if (expected > 0x3fffffff) expected = 0x3fffffff;
        }
        q.arm(a, T{D{w}}); require(q.timeout_ms(T{D{n}}) == static_cast<int>(expected)); q.cancel(a);
    }
    std::puts("allocation rollback/expiry retry; 20000 independent wide numeric cases passed");
 } catch (const std::exception& e) { allocations = -1; std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for name,header,queue,token,tokens in [
  ('root',ROOT/'server/timer_queue.h','relay::TimerQueue','void*','int x,y; Token a=&x,b=&y;'),
  ('teaching',CP/'net/timer_heap.h','study_net::TimerHeap','std::uint64_t','Token a=1,b=2;')]:
  src=OUT/(name+'.cpp');src.write_text(SOURCE.replace('HEADER',str(header)).replace('QUEUE',queue).replace('TOKENS',tokens).replace('TOKEN',token))
  exe=OUT/name
  run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-fsanitize=undefined','-fno-sanitize-recover=all','-I'+str(CP),str(src),'-o',str(exe)])
  print(name,run([str(exe)]).stdout,flush=True)
if __name__=='__main__':main()
