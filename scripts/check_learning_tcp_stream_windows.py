"""API-double checks of Windows transfer branches, not a Windows SDK/native test."""
from pathlib import Path
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/84-tcp-stream-check/windows-double'
CP=ROOT/'docs/learn/checkpoints/84-tcp-stream'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 (OUT/'winsock2.h').write_text(r'''
#pragma once
#include <cstdint>
#include <cstdlib>
using SOCKET=std::uintptr_t;
constexpr int SOCKET_ERROR=-1,WSAEINTR=10004,WSAEWOULDBLOCK=10035,WSAEINPROGRESS=10036,WSAEINVAL=10022,WSAECONNABORTED=10053,SD_SEND=1;
namespace fake {
inline constexpr SOCKET handle=SOCKET{1}<<40;
inline int calls=0,code=WSAEINTR,last_length=0;
inline void reset(int error=WSAEINTR) { calls=0;code=error;last_length=0; }
inline void observe(SOCKET s,int n) {
 if(s!=handle || ++calls>1) std::abort(); // cancellation must not be retried
 last_length=n;
}
}
inline int WSAGetLastError() { return fake::code; }
inline int send(SOCKET s,const char*,int n,int) { fake::observe(s,n);return SOCKET_ERROR; }
inline int recv(SOCKET s,char*,int n,int) { fake::observe(s,n);return SOCKET_ERROR; }
inline int shutdown(SOCKET s,int how) { fake::observe(s,how);return SOCKET_ERROR; }
''')
 (OUT/'ws2tcpip.h').write_text('#pragma once\n')
 common=r'''
#include <winsock2.h>
#include <cstdio>
#include <limits>
#define CHECK(x) do { if(!(x)) std::abort(); } while(0)
'''
 source=(ROOT/'net/socket.cpp').read_text()
 text=common+'''
#include "net/socket.h"
#include "net/io_size.h"
#include <chrono>
#include <thread>
namespace net {
'''+ '\n'.join(cut(source,s) for s in ['bool tcp_send_some','bool tcp_send_all','bool tcp_recv_some'])+r'''
}
int main() {
 net::TcpSocket socket; socket.fdh=std::make_shared<net::NativeSocket>(fake::handle);
 const char byte='A';size_t accepted=99;
 const size_t huge=static_cast<size_t>((std::numeric_limits<int>::max)())+10;
 // The double only observes pointer/length; it does not read a huge buffer.
 fake::reset();CHECK(!net::tcp_send_some(socket,&byte,huge,accepted));
 CHECK(accepted==0 && fake::calls==1 && fake::last_length==(std::numeric_limits<int>::max)());
 fake::reset();CHECK(!net::tcp_send_all(socket,&byte,huge) && fake::calls==1);
 std::vector<uint8_t> data{42};
 fake::reset();CHECK(!net::tcp_recv_some(socket,data) && fake::calls==1 && data.size()==1);
 fake::reset(WSAEINPROGRESS);CHECK(!net::tcp_recv_some(socket,data) && fake::calls==1);
 fake::reset(WSAEWOULDBLOCK);CHECK(net::tcp_recv_some(socket,data) && data.size()==1);
 std::puts("Windows root transfer branches: full-width handle, clamp and cancellation passed (API double).");
}
'''
 (OUT/'root.cpp').write_text(text)
 flags=['c++','-std=c++17','-Wall','-Wextra','-Wpedantic','-D_WIN32','-I'+str(OUT)]
 run([*flags,'-I'+str(ROOT),str(OUT/'root.cpp'),'-o',str(OUT/'root')]);print(run([str(OUT/'root')]).stdout,flush=True)
 (OUT/'study.cpp').write_text(common+r'''
#include "net/stream.h"
namespace study_net { void Socket::reset() noexcept { (void)release(); } }
int main() {
 study_net::Socket socket(fake::handle);
 uint8_t byte=42;
 const size_t huge=static_cast<size_t>((std::numeric_limits<int>::max)())+10;
 fake::reset();auto r=study_net::send_some(socket,&byte,huge);
 CHECK(r.status==study_net::StreamStatus::error && r.error==WSAEINTR && r.count==0);
 CHECK(fake::calls==1 && fake::last_length==(std::numeric_limits<int>::max)());
 fake::reset();r=study_net::receive_some(socket,&byte,1);
 CHECK(r.status==study_net::StreamStatus::error && r.error==WSAEINTR && fake::calls==1);
 fake::reset();int error=0;CHECK(!study_net::shutdown_send(socket,error) && error==WSAEINTR && fake::last_length==SD_SEND);
 std::puts("Windows study stream.cpp: cancellation and request clamp passed (API double).");
}
''')
 run([*flags,'-I'+str(CP),str(CP/'net/stream.cpp'),str(OUT/'study.cpp'),'-o',str(OUT/'study')]);print(run([str(OUT/'study')]).stdout,flush=True)
if __name__=='__main__':main()
