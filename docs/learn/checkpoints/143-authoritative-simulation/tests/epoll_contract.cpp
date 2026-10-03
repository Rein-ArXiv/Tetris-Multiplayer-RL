#include "tests/reactor_fixture.h"
#include "net/epoll_reactor.h"
#include "net/unique_fd.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstdio>
#include <cerrno>
using namespace study_net;
using namespace study_fixture;
namespace {
int fail_operation = -1;
}
extern "C" int __real_epoll_ctl(int, int, int, epoll_event*);
extern "C" int __wrap_epoll_ctl(int epfd, int op, int fd, epoll_event* event) {
    if (op == fail_operation) { fail_operation = -1; errno = ENOSPC; return -1; }
    return __real_epoll_ctl(epfd, op, fd, event);
}
namespace {
void fd_ownership() {
    UniqueFd first(::eventfd(0,EFD_CLOEXEC)); require(first.valid(),"eventfd");
    const int fd=first.get(); UniqueFd second(std::move(first));
    require(!first.valid()&&second.get()==fd,"move fd owner");
    second.reset(fd); require(::fcntl(fd,F_GETFD)!=-1,"self reset keeps fd");
    errno=ERANGE; second.reset(); require(errno==ERANGE,"cleanup preserves errno");
    require(::fcntl(fd,F_GETFD)==-1&&errno==EBADF,"closed once");
    UniqueFd released(::eventfd(0,EFD_CLOEXEC)); const int raw=released.release();
    require(!released.valid()&&::fcntl(raw,F_GETFD)!=-1,"release leaves fd alive");
    UniqueFd owner(raw);
}
void raw_trigger(bool edge, bool oneshot) {
    int ends[2]; require(::pipe2(ends,O_NONBLOCK|O_CLOEXEC)==0,"pipe");
    UniqueFd read_end(ends[0]),write_end(ends[1]),epoll(::epoll_create1(EPOLL_CLOEXEC));
    require(epoll.valid(),"epoll");
    epoll_event wanted{}; wanted.events=EPOLLIN|(edge?EPOLLET:0u)|(oneshot?EPOLLONESHOT:0u);
    wanted.data.u64=77;
    require(::epoll_ctl(epoll.get(),EPOLL_CTL_ADD,read_end.get(),&wanted)==0,"raw add");
    require(::write(write_end.get(),"abcdefgh",8)==8,"pipe data already queued");
    epoll_event event{};
    require(::epoll_wait(epoll.get(),&event,1,0)==1&&event.data.u64==77,"initial ready identity");
    char bytes[8]; require(::read(read_end.get(),bytes,1)==1,"partial read");
    const int again=::epoll_wait(epoll.get(),&event,1,0);
    require(again==((edge||oneshot)?0:1),"LT repeats, ET/oneshot do not repeat this unchanged pipe");
    if(oneshot) {
        require(::epoll_ctl(epoll.get(),EPOLL_CTL_MOD,read_end.get(),&wanted)==0,"rearm");
        require(::epoll_wait(epoll.get(),&event,1,0)==1,"rearm observes remaining bytes");
    }
    require(::read(read_end.get(),bytes,7)==7,"remaining seven bytes");
    require(::read(read_end.get(),bytes,1)==-1&&errno==EAGAIN,"drained to would block");
    require(::epoll_ctl(epoll.get(),EPOLL_CTL_DEL,read_end.get(),nullptr)==0,"raw remove");
    require(::write(write_end.get(),"z",1)==1,"new input after remove");
    require(::epoll_wait(epoll.get(),&event,1,0)==0,"removed entry absent");
}
void zero_mask_is_not_pause() {
    int ends[2];require(::pipe2(ends,O_NONBLOCK|O_CLOEXEC)==0,"pipe");
    UniqueFd reader(ends[0]),writer(ends[1]),epoll(::epoll_create1(EPOLL_CLOEXEC));
    epoll_event wanted{};wanted.data.u64=9; // Deliberately request no normal interests.
    require(::epoll_ctl(epoll.get(),EPOLL_CTL_ADD,reader.get(),&wanted)==0,"zero mask add");
    require(::write(writer.get(),"hi",2)==2,"bytes before hangup");
    writer.reset();epoll_event event{};
    require(::epoll_wait(epoll.get(),&event,1,0)==1&&(event.events&EPOLLHUP),"HUP not masked by zero interest");
    char remaining[2];require(::read(reader.get(),remaining,2)==2,"HUP can coexist with unread bytes");
    require(::epoll_ctl(epoll.get(),EPOLL_CTL_DEL,reader.get(),nullptr)==0,"detach paused");
    require(::epoll_wait(epoll.get(),&event,1,0)==0,"DEL actually stops observation");
}
void control_failures() {
    auto reactor=make_epoll_reactor();require(bool(reactor),"factory");auto sockets=pair();
    fail_operation=EPOLL_CTL_ADD;
    require(!reactor->watch(sockets.receiver,Read),"failed ADD rejected");
    auto id=reactor->watch(sockets.receiver,Read);
    require(id&&*id==2,"failed ADD consumed ID but released slot and fd membership");
    send(sockets.sender,7);observe_until(*reactor,1);
    fail_operation=EPOLL_CTL_DEL;
    require(!reactor->change(*id,0)&&reactor->current(*id),"failed pause preserves old membership");
    require(observe_until(*reactor,1).events[0].id==*id,"old read interest still active");
    require(reactor->change(*id,0),"pause after failed DEL");
    fail_operation=EPOLL_CTL_ADD;
    require(!reactor->change(*id,Read)&&reactor->poll(0).state==PollState::timeout,"failed resume stays paused");
    require(reactor->change(*id,Read),"resume success");
    fail_operation=EPOLL_CTL_MOD;
    require(!reactor->change(*id,Write),"failed MOD");
    auto unchanged=observe_until(*reactor,1);require(unchanged.events[0].readable&&!unchanged.events[0].writable,"failed MOD keeps original read mask");
    fail_operation=EPOLL_CTL_DEL;
    require(!reactor->unwatch(*id)&&reactor->current(*id),"failed removal stays registered");
    require(reactor->unwatch(*id)&&!reactor->current(*id),"removal succeeds on retry");
}
void eof_and_wake() {
    auto reactor=make_epoll_reactor();require(bool(reactor),"factory");auto sockets=pair();
    auto id=reactor->watch(sockets.receiver,0);require(bool(id),"paused initial registration");
    send(sockets.sender,13);send(sockets.sender,14);
    require(::shutdown(sockets.sender.native(),SHUT_WR)==0,"half close");
    require(reactor->poll(0).state==PollState::timeout,"paused: no data or half-close event");
    require(reactor->change(*id,Read),"resume");
    unsigned total=0;bool eof=false;std::uint8_t byte;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!eof&&std::chrono::steady_clock::now()<deadline) {
        auto batch=reactor->poll(20);require(batch.state!=PollState::error,"poll halfclose");
        if(batch.events.empty())continue;
        const auto got=try_receive(sockets.receiver,&byte,1);
        if(got.state==ReceiveState::progress){require(byte==13+total,"ordered bytes before EOF");++total;}
        else if(got.state==ReceiveState::eof)eof=true;
        else require(got.state==ReceiveState::would_block||got.state==ReceiveState::interrupted,"receive");
    }
    require(total==2&&eof,"data then EOF");
    send(sockets.receiver,42); // Peer closed its write direction, not its read direction.
    bool reply=false;const auto reply_deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!reply&&std::chrono::steady_clock::now()<reply_deadline) {
        const auto got=try_receive(sockets.sender,&byte,1);
        if(got.state==ReceiveState::progress){require(byte==42,"reverse data");reply=true;}
        else require(got.state==ReceiveState::would_block||got.state==ReceiveState::interrupted,"reverse receive");
    }
    require(reply,"half-close leaves reverse direction usable");
    require(reactor->unwatch(*id),"remove EOF");
    for(unsigned i=0;i<100;++i)require(reactor->wake(),"repeated wake");
    auto batch=reactor->poll(1000);require(batch.woken&&batch.events.empty(),"coalesced wakes");
    require(reactor->poll(0).state==PollState::timeout,"one read cleared quiescent counter");
}
}
int main() {
    Runtime runtime;if(!runtime.ready())return 2;
    try {fd_ownership();raw_trigger(false,false);raw_trigger(true,false);raw_trigger(false,true);
         zero_mask_is_not_pause();control_failures();eof_and_wake();}
    catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
    std::puts("epoll: ownership, LT/ET/ONESHOT, zero-mask HUP, transactional ADD/MOD/DEL, paused EOF, coalesced wake passed");
}
