// Actual socket.cpp with a slow, intermittently progressing send syscall.
#include "net/socket.cpp"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace {
using Clock=std::chrono::steady_clock;
Clock::time_point began;
unsigned char payload[100]{};
size_t accepted=0,calls=0;
}
extern "C" ssize_t __wrap_send(int,const void* bytes,size_t size,int) {
    CHECK(bytes==payload+accepted && size==sizeof(payload)-accepted);++calls;
    const auto elapsed=Clock::now()-began;
    // One byte every 1.5s: an inactivity-only timer never reaches five seconds.
    if(elapsed>=std::chrono::milliseconds(1500*accepted)) {++accepted;return 1;}
    errno=EAGAIN;return -1;
}
int main() {
    auto socket=net::make_owned(::socket(AF_INET,SOCK_STREAM,0));CHECK(socket.valid());
    began=Clock::now();CHECK(!net::tcp_send_all(socket,payload,sizeof(payload)));
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-began);
    CHECK(accepted>0 && accepted<sizeof(payload) && elapsed.count()>=5000);
    std::printf("Total deadline: returned after %lldms, accepted=%zu, attempts=%zu despite intermittent progress\n",static_cast<long long>(elapsed.count()),accepted,calls);
}
