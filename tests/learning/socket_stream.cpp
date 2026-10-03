// Deterministic syscall outcomes in the actual production socket implementation.
// No kernel packet grouping is asserted by this test.
#include "net/socket.cpp"
#include <array>
#include <cstdlib>
#include <limits>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
namespace {
struct Step { ssize_t count; int error; };
std::array<Step,8> steps{};
std::size_t used=0, calls=0, transferred=0;
const unsigned char message[]={'A','B','C','D','E','F'};
void plan(std::initializer_list<Step> next) {
    used=next.size(); calls=transferred=0;
    std::copy(next.begin(),next.end(),steps.begin());
}
Step next_step() { CHECK(calls<used); return steps[calls++]; }
}
extern "C" ssize_t __wrap_send(int,const void* data,size_t len,int) {
    CHECK(data==message+transferred && len==6-transferred);
    Step s=next_step();
    if(s.count>0) { CHECK(static_cast<size_t>(s.count)<=len);transferred+=s.count; }
    errno=s.error;return s.count;
}
extern "C" ssize_t __wrap_recv(int,void* data,size_t len,int) {
    CHECK(len==4096);
    Step s=next_step();
    if(s.count>0) { CHECK(s.count==3);unsigned char bytes[]={0,255,42};std::memcpy(data,bytes,3); }
    errno=s.error;return s.count;
}
int main() {
    constexpr size_t max=static_cast<size_t>((std::numeric_limits<int>::max)());
    static_assert(net::io_chunk_size(0)==0);
    static_assert(net::io_chunk_size(max)==static_cast<int>(max));
    static_assert(net::io_chunk_size(max+1)==static_cast<int>(max));
    static_assert(net::io_chunk_size((std::numeric_limits<size_t>::max)())==static_cast<int>(max));
    auto s=net::make_owned(::socket(AF_INET,SOCK_STREAM,0));CHECK(s.valid());
    size_t sent=99;
    plan({{2,0},{-1,EINTR},{1,0},{-1,EAGAIN}});
    CHECK(net::tcp_send_some(s,message,6,sent) && sent==3 && calls==4);
    plan({{2,0},{-1,ECONNRESET}});
    CHECK(!net::tcp_send_some(s,message,6,sent) && sent==2);
    plan({{2,0},{0,0}});
    CHECK(!net::tcp_send_some(s,message,6,sent) && sent==2);
    plan({{-1,EAGAIN}});
    CHECK(net::tcp_send_some(s,message,6,sent) && sent==0);
    plan({{2,0},{-1,EINTR},{4,0}});
    CHECK(net::tcp_send_all(s,message,6) && transferred==6);
    std::vector<uint8_t> got{7};
    plan({{-1,EINTR},{3,0}});
    CHECK(net::tcp_recv_some(s,got) && calls==2);
    CHECK((got==std::vector<uint8_t>{7,0,255,42}));
    for(auto step: {Step{-1,EAGAIN},Step{0,0},Step{-1,ECONNRESET}}) {
        auto before=got;plan({step});
        CHECK(net::tcp_recv_some(s,got)==(step.error==EAGAIN));
        CHECK(got==before);
    }
    std::puts("Root stream: partial progress on failure, interrupted receive, binary bytes and narrowing limits passed.");
}
