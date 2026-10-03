"""Compile lesson87's Windows adapter against an API double; not native SDK evidence."""
from pathlib import Path
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1]
CP=ROOT/'docs/learn/checkpoints/87-partial-send'
OUT=ROOT/'out/learning-checkpoints/87-partial-send-check/windows-double'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 (OUT/'winsock2.h').write_text(r'''
#pragma once
#include <cstdint>
#include <cstdlib>
using SOCKET=std::uintptr_t;using u_long=unsigned long;
constexpr int SOCKET_ERROR=-1,WSAEINTR=10004,WSAEWOULDBLOCK=10035,WSAEINVAL=10022,WSAECONNABORTED=10053;
constexpr long FIONBIO=1;
namespace fake {
inline constexpr SOCKET handle=SOCKET{1}<<40;
inline int calls=0,code=WSAEINTR,result=-1,length=0,mode=-1,io_result=0;
}
inline int WSAGetLastError(){return fake::code;}
inline int send(SOCKET socket,const char*,int size,int){if(socket!=fake::handle)std::abort();++fake::calls;fake::length=size;return fake::result;}
inline int ioctlsocket(SOCKET socket,long command,u_long* mode){if(socket!=fake::handle||command!=FIONBIO)std::abort();fake::mode=int(*mode);return fake::io_result;}
''')
 (OUT/'ws2tcpip.h').write_text('#pragma once\n')
 (OUT/'check.cpp').write_text(r'''
#include <winsock2.h>
#include "net/send_socket.h"
#include <limits>
#include <cstdio>
#define CHECK(x) do{if(!(x))std::abort();}while(0)
namespace study_net { void Socket::reset() noexcept{(void)release();} }
int main(){using namespace study_net;Socket socket(fake::handle);int error=99;
 CHECK(set_nonblocking(socket,true,error)&&error==0&&fake::mode==1);
 CHECK(set_nonblocking(socket,false,error)&&fake::mode==0);
 fake::io_result=-1;CHECK(!set_nonblocking(socket,true,error)&&error==WSAEINTR);
 const std::uint8_t byte=1;const auto huge=std::size_t((std::numeric_limits<int>::max)())+1;
 auto r=try_send(socket,&byte,huge);CHECK(r.state==SendState::error&&r.error==WSAEINTR&&fake::calls==1&&fake::length==(std::numeric_limits<int>::max)());
 fake::code=WSAEWOULDBLOCK;r=try_send(socket,&byte,1);CHECK(r.state==SendState::would_block&&r.count==0&&r.error==0);
 fake::result=1;r=try_send(socket,&byte,1);CHECK(r.state==SendState::progress&&r.count==1);
 fake::result=0;fake::code=123;r=try_send(socket,&byte,1);CHECK(r.state==SendState::error&&r.error==WSAECONNABORTED);
 const int before=fake::calls;r=try_send(socket,nullptr,1);CHECK(r.error==WSAEINVAL&&fake::calls==before);
 std::puts("Windows adapter double: full-width handle, mode/failed ioctl, clamp, would-block, cancellation and zero-send passed");}
''')
 run(['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-D_WIN32','-I'+str(OUT),'-I'+str(CP),str(CP/'net/send_socket.cpp'),str(OUT/'check.cpp'),'-pthread','-o',str(OUT/'check')]);print(run([str(OUT/'check')]).stdout,flush=True)
if __name__=='__main__':main()
