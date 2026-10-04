// Linux fault injection into the actual production ownership/setup functions.
#include <cstdlib>
#include <new>
#include <cstdarg>
static int fail_allocation = 0;
void* operator new(std::size_t size) {
    if (fail_allocation > 0 && --fail_allocation == 0) throw std::bad_alloc();
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
#include "net/socket.cpp"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"failed line %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
static int closes = 0;
static bool fail_option = false, fail_mode = false;
extern "C" int __real_close(int);
extern "C" int __wrap_close(int fd) { ++closes; return __real_close(fd); }
extern "C" int __real_setsockopt(int,int,int,const void*,socklen_t);
extern "C" int __wrap_setsockopt(int fd,int level,int opt,const void* p,socklen_t n) {
    if (fail_option && opt == SO_REUSEADDR) { errno = ENOPROTOOPT; return -1; }
    return __real_setsockopt(fd,level,opt,p,n);
}
extern "C" int __real_fcntl(int,int,...);
extern "C" int __wrap_fcntl(int fd,int cmd,...) {
    if (fail_mode && cmd == F_GETFL) { errno = EIO; return -1; }
    if (cmd == F_SETFL) { va_list args; va_start(args,cmd); int arg=va_arg(args,int); va_end(args); return __real_fcntl(fd,cmd,arg); }
    return __real_fcntl(fd,cmd);
}
int main() {
    for (int allocation : {1,2}) {
        int fd = ::socket(AF_INET,SOCK_STREAM,0); CHECK(fd >= 0);
        closes = 0; fail_allocation = allocation;
        auto owner = net::make_owned(fd);
        CHECK(!owner.valid() && closes == 1);
        CHECK(::fcntl(fd,F_GETFD) == -1 && errno == EBADF);
    }
    {
        auto one=net::make_owned(::socket(AF_INET,SOCK_STREAM,0)); CHECK(one.valid());
        closes=0; auto two=one; one={}; CHECK(two.valid() && closes==0);
        two={}; CHECK(closes==1);
    }
    fail_option=true; closes=0;
    CHECK(!net::tcp_listen(0,1,true).valid() && closes==1);
    fail_option=false;
    auto listener=net::tcp_listen(0,1,true); CHECK(listener.valid());
    sockaddr_in addr{}; socklen_t len=sizeof(addr);
    CHECK(getsockname(listener.fd(),reinterpret_cast<sockaddr*>(&addr),&len)==0);
    net::TcpSocket invalid;
    CHECK(!net::tcp_set_nonblocking(invalid));
    fail_mode=true;
    CHECK(!net::tcp_set_nonblocking(listener));
    fail_mode=false;
    CHECK(net::tcp_set_nonblocking(listener));
    CHECK((::fcntl(listener.fd(),F_GETFL) & O_NONBLOCK) != 0);
    fail_mode=true; closes=0;
    CHECK(!net::tcp_connect("127.0.0.1",ntohs(addr.sin_port)).valid());
    CHECK(closes==1);
    net::AcceptResult result=net::AcceptResult::Ok; closes=0;
    CHECK(!net::tcp_accept(listener,&result).valid());
    CHECK(result==net::AcceptResult::Error && closes==1);
    fail_mode=false;
    listener={}; net::net_shutdown();
    std::puts("Root: both allocation failures, shared ownership, bind option and nonblocking failures passed.");
}
