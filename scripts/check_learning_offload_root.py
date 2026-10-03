"""Current/teaching offload: fault injection, allocation-free publication, startup unwind."""
from pathlib import Path
import subprocess,sys
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];CP=ROOT/'docs/learn/checkpoints/130-offload';OUT=ROOT/'out/learning-checkpoints/130-offload-faults'
FAULTS=r'''
#include "HEADER"
#include <cstdlib>
#include <new>
#include <array>
#include <cstdio>
static thread_local int allocation_budget = -1;
void* operator new(std::size_t n) {
    if (allocation_budget == 0) throw std::bad_alloc();
    if (allocation_budget > 0) --allocation_budget;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
using Pool = POOL;
void require(bool ok) { if (!ok) throw std::runtime_error("offload fault contract"); }
int main() {
 try {
    Pool pool(1, {}, 2);
    Pool::Job job = [] { return [] {}; };
    bool failed = false; allocation_budget = 0;
    try { pool.submit(std::move(job)); } catch (const std::bad_alloc&) { failed = true; }
    allocation_budget = -1; require(failed);
    int applied = 0;
    require(pool.submit([&] { return [&] { applied += 1; }; }));
    require(pool.submit([&] { return [&] { applied += 2; }; }));
    pool.shutdown();
    std::vector<Pool::Cont> out;
    allocation_budget = 0; failed = false;
    try { pool.drain(out); } catch (const std::bad_alloc&) { failed = true; }
    allocation_budget = -1; require(failed && out.empty());
    require(pool.drain(out) == 2); for (auto& c : out) c(); require(applied == 3);
    {
        Pool worker(1, {});
        std::array<int, 128> large{}; large[127] = 42;
        Pool::Cont result = [&, large] { applied += large[127]; };
        require(worker.submit([result = std::move(result)]() mutable {
            allocation_budget = 0; // Only this worker: publishing must not allocate a queue node.
            return std::move(result);
        }));
        worker.shutdown(); out.clear(); require(worker.drain(out) == 1); out[0]();
        require(applied == 45);
    }
    {
        Pool worker(1, {});
        require(worker.submit([]()->Pool::Cont { throw std::runtime_error("expected"); }));
        worker.shutdown(); out.clear(); out.reserve(1);
        allocation_budget = 0; failed = false;
        try { worker.drain(out); } catch (const std::bad_alloc&) { failed = true; }
        allocation_budget = -1;
        // std::function may store this closure inline on some standard libraries.
        if (failed) { require(out.empty()); require(worker.drain(out) == 1); }
        require(out.size() == 1);
        bool restored = false;
        try { out[0](); } catch (const std::runtime_error&) { restored = true; }
        require(restored);
    }
    std::puts("submit/drain allocation failure preserved; worker publication without allocation; error retained");
 } catch (const std::exception& e) { allocation_budget = -1; std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
'''
STARTUP=r'''
#include "HEADER"
#include <dlfcn.h>
#include <pthread.h>
#include <cerrno>
#include <cstdio>
#include <system_error>
static bool injecting = false;
static int creates = 0, joins = 0;
extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attrs,
                               void*(*entry)(void*), void* arg) {
    using Fn = int(*)(pthread_t*, const pthread_attr_t*, void*(*)(void*), void*);
    static auto actual = reinterpret_cast<Fn>(dlsym(RTLD_NEXT,"pthread_create"));
    if (injecting && ++creates == 2) return EAGAIN;
    return actual(thread, attrs, entry, arg);
}
extern "C" int pthread_join(pthread_t thread, void** result) {
    using Fn = int(*)(pthread_t, void**);
    static auto actual = reinterpret_cast<Fn>(dlsym(RTLD_NEXT,"pthread_join"));
    if (injecting) ++joins;
    return actual(thread,result);
}
int main() {
    injecting = true; bool caught = false;
    try { POOL pool(3, {}); } catch (const std::system_error&) { caught = true; }
    injecting = false;
    if (!caught || creates != 2 || joins != 1) return 1;
    std::puts("second thread startup rejected; first worker joined before rethrow");
}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for name,header,pool in [('root',ROOT/'server/offload.h','relay::Offload'),('teaching',CP/'net/offload.h','study_net::Offload')]:
  for suffix,source in [('faults',FAULTS),('startup',STARTUP)]:
   src=OUT/f'{name}-{suffix}.cpp';src.write_text(source.replace('HEADER',str(header)).replace('POOL',pool));exe=src.with_suffix('')
   args=['c++','-std=c++17','-pthread','-Wall','-Wextra','-fsanitize=undefined','-fno-sanitize-recover=all',str(src),'-o',str(exe)]
   if suffix=='startup':args+=['-Wl,--export-dynamic','-ldl']
   run(args);print(name,suffix,run([str(exe)],timeout=10).stdout,flush=True)
 if '--before' in sys.argv:
  header=ROOT/'out/learning-jobs/130-offload-before.h';src=OUT/'before-startup.cpp';src.write_text(STARTUP.replace('HEADER',str(header)).replace('POOL','relay::Offload'));exe=src.with_suffix('')
  run(['c++','-std=c++17','-pthread',str(src),'-Wl,--export-dynamic','-ldl','-o',str(exe)])
  p=subprocess.run([str(exe)],text=True,capture_output=True,timeout=10)
  assert p.returncode!=0;(ROOT/'out/learning-jobs/130-startup-before.log').write_text(p.stdout+p.stderr+f'code={p.returncode}\n');print('before startup terminated; evidence saved',flush=True)
if __name__=='__main__':main()
