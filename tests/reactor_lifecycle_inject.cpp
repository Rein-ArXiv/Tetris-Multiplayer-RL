// Linux-only LD_PRELOAD fixture: real pthread creation/join and loop exceptions.
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <new>
#include <pthread.h>
#include <sys/epoll.h>

namespace {
std::atomic<int> creates{0}, started{0}, completed{0}, joined{0};
std::atomic<bool> threw{false};
const pthread_t main_thread = pthread_self();
struct Start { void* (*entry)(void*); void* argument; };
void* tracked(void* argument) {
    const Start start = *static_cast<Start*>(argument);
    std::free(argument);
    void* result = start.entry(start.argument);
    ++completed;
    return result;
}
__attribute__((destructor)) void report() {
    std::fprintf(stderr, "lifecycle creates=%d started=%d completed=%d joined=%d\n",
                 creates.load(), started.load(), completed.load(), joined.load());
}
}
extern "C" int pthread_create(pthread_t* thread, const pthread_attr_t* attributes,
                               void* (*entry)(void*), void* argument) {
    using Function = int (*)(pthread_t*, const pthread_attr_t*, void* (*)(void*), void*);
    static auto actual = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "pthread_create"));
    const int attempt = ++creates;
    const char* failure = std::getenv("RELAY_TEST_FAIL_CREATE");
    if (failure && attempt == std::atoi(failure)) return EAGAIN;
    auto* start = static_cast<Start*>(std::malloc(sizeof(Start)));
    if (!start) return ENOMEM;
    *start = {entry, argument};
    const int result = actual(thread, attributes, tracked, start);
    if (result == 0) ++started;
    else std::free(start);
    return result;
}
extern "C" int pthread_join(pthread_t thread, void** result) {
    using Function = int (*)(pthread_t, void**);
    static auto actual = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "pthread_join"));
    const int status = actual(thread, result);
    if (status == 0) ++joined;
    return status;
}
extern "C" int epoll_wait(int fd, epoll_event* events, int count, int timeout) {
    using Function = int (*)(int, epoll_event*, int, int);
    static auto actual = reinterpret_cast<Function>(dlsym(RTLD_NEXT, "epoll_wait"));
    const char* target = std::getenv("RELAY_TEST_THROW_POLL");
    const bool front = pthread_equal(pthread_self(), main_thread);
    if (target && ((target[0] == 'f' && front) || (target[0] == 's' && !front))
        && !threw.exchange(true)) throw std::bad_alloc();
    return actual(fd, events, count, timeout);
}
