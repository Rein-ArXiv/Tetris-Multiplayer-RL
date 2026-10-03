// Fail one allocation per process; the caller checks timeout as well as exit.
#include "server/ip_admission.h"
#include "server/player_session.h"
#include <cstdlib>
#include <cstdio>
#include <new>
#include <string>
static thread_local long fail_after = -1;
void* operator new(std::size_t n) {
    if (fail_after >= 0 && fail_after-- == 0) {
        fail_after = -1;
        throw std::bad_alloc();
    }
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p, std::size_t) noexcept {std::free(p);}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete[](void* p, std::size_t) noexcept {std::free(p);}
int main(int argc,char** argv) {
    if (argc != 3) return 2;
    const std::string mode=argv[1];const long index=std::strtol(argv[2],nullptr,10);
    bool failed=false;
    if(mode=="player") {
        fail_after=index;
        try {auto lease=relay::PlayerSessionLease::acquire(77);}
        catch(const std::bad_alloc&) {failed=true;}
        fail_after=-1;
        auto retry=relay::PlayerSessionLease::acquire(77);
        if(!retry) {std::fprintf(stderr,"player reservation leaked after allocation %ld\n",index);return 1;}
        if(relay::PlayerSessionLease::acquire(77)) return 1;
        auto alias=retry;retry.reset();if(relay::PlayerSessionLease::acquire(77))return 1;
        alias.reset();if(!relay::PlayerSessionLease::acquire(77))return 1;
    } else {
        using Ip=relay::IpAdmission;
        Ip::set_session_limit(2);
        const std::string key="local";
        auto held= mode=="existing" ? Ip::acquire(key,Ip::Kind::Session) : std::shared_ptr<Ip>{};
        fail_after=index;
        try {auto lease=Ip::acquire(key,Ip::Kind::Session);}
        catch(const std::bad_alloc&) {failed=true;}
        fail_after=-1;
        auto one=Ip::acquire(key,Ip::Kind::Session);
        auto two=held ? std::shared_ptr<Ip>{} : Ip::acquire(key,Ip::Kind::Session);
        if(!one || (!held && !two)) {std::fprintf(stderr,"IP reservation leaked after allocation %ld\n",index);return 1;}
        if(Ip::acquire(key,Ip::Kind::Session)) return 1;
        auto alias=one;one.reset();if(Ip::acquire(key,Ip::Kind::Session))return 1;
        alias.reset();if(!Ip::acquire(key,Ip::Kind::Session))return 1;
    }
    std::printf("%s allocation %ld: %s, capacity restored\n",mode.c_str(),index,failed?"failed":"completed");
}
