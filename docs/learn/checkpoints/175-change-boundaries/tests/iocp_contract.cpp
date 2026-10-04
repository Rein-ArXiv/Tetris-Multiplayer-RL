#include <winsock2.h>
#include "net/iocp_receive.h"
#include <deque>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string_view>
using namespace study_net;
namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
enum class SubmitMode{pending,immediate,rejected};
struct Packet{BOOL success;DWORD count,error;ULONG_PTR key;OVERLAPPED* operation;};
struct Port {
    SOCKET socket;ULONG_PTR key;std::deque<Packet> packets;
    OVERLAPPED* borrowed=nullptr;char* buffer=nullptr;DWORD capacity=0;
    bool accepted_uncollected=false,broken=false;
};
std::vector<Port*> ports;DWORD last_error=0;SubmitMode mode=SubmitMode::pending;
unsigned closed_ports=0,closed_sockets=0;bool early_release=false;SOCKET next_socket=100;
Port* last_port(){require(!ports.empty(),"port exists");return ports.back();}
void finish(Port& port,BOOL success,DWORD count,DWORD error) {
    require(port.borrowed&&port.accepted_uncollected,"finish accepted operation");
    if(success)for(DWORD i=0;i<count&&i<port.capacity;++i)port.buffer[i]=static_cast<char>(11+i);
    port.packets.push_back({success,count,error,port.key,port.borrowed});
    port.borrowed=nullptr; // No OS writing remains, but packet ownership is still outstanding.
}
Socket socket(){return Socket(next_socket++);}
}
HANDLE CreateIoCompletionPort(HANDLE socket,HANDLE,ULONG_PTR key,DWORD){
 auto* p=new Port{};p->socket=reinterpret_cast<SOCKET>(socket);p->key=key;ports.push_back(p);return p;
}
BOOL CloseHandle(HANDLE handle){
 auto* p=static_cast<Port*>(handle);early_release|=p->accepted_uncollected;
 ports.erase(std::find(ports.begin(),ports.end(),p));delete p;++closed_ports;return TRUE;
}
DWORD GetLastError(){return last_error;}
int WSAGetLastError(){return static_cast<int>(last_error);}
int WSARecv(SOCKET fd,WSABUF* buffer,DWORD n,DWORD* count,DWORD*,OVERLAPPED* operation,void*) {
 auto it=std::find_if(ports.begin(),ports.end(),[=](auto*p){return p->socket==fd;});require(it!=ports.end(),"associated socket");
 auto& p=**it;require(n==1&&count==nullptr&&buffer->len>0&&!p.accepted_uncollected,"submission parameters/lifetime");
 if(mode==SubmitMode::rejected){last_error=WSAECONNRESET;return -1;}
 p.borrowed=operation;p.buffer=buffer->buf;p.capacity=buffer->len;p.accepted_uncollected=true;
 if(mode==SubmitMode::immediate){finish(p,TRUE,1,0);return 0;}
 last_error=WSA_IO_PENDING;return -1;
}
BOOL GetQueuedCompletionStatus(HANDLE handle,DWORD* count,ULONG_PTR* key,OVERLAPPED** operation,DWORD wait) {
 auto& p=*static_cast<Port*>(handle);*count=0;*key=0;*operation=nullptr;
 if(p.broken){last_error=ERROR_INVALID_HANDLE;return FALSE;}
 if(p.packets.empty()){
   require(wait!=INFINITE,"no infinite wait may release a still-pending operation");
   last_error=WAIT_TIMEOUT;return FALSE;
 }
 const auto packet=p.packets.front();p.packets.pop_front();
 *count=packet.count;*key=packet.key;*operation=packet.operation;
 if(packet.operation)p.accepted_uncollected=false;
 last_error=packet.error;return packet.success;
}
BOOL PostQueuedCompletionStatus(HANDLE handle,DWORD count,ULONG_PTR key,OVERLAPPED* operation){
 static_cast<Port*>(handle)->packets.push_back({TRUE,count,0,key,operation});return TRUE;
}
BOOL CancelIoEx(HANDLE fd,OVERLAPPED* operation){
 auto it=std::find_if(ports.begin(),ports.end(),[=](auto*p){return p->socket==reinterpret_cast<SOCKET>(fd);});
 require(it!=ports.end(),"cancel associated socket");auto& p=**it;
 if(!p.borrowed){last_error=ERROR_NOT_FOUND;return FALSE;}
 require(p.borrowed==operation,"cancel right operation");finish(p,FALSE,0,ERROR_OPERATION_ABORTED);return TRUE;
}
namespace study_net {
void Socket::reset() noexcept {
 if(!valid())return;
 for(auto* p:ports)if(p->socket==native_)early_release|=p->accepted_uncollected;
 native_=Runtime::kInvalid;++closed_sockets;
}
}
int main(int argc,char**argv){
 try {
  if(argc==2&&std::string_view(argv[1])=="--broken-port") {
    Port* retained=nullptr;const auto closed=closed_ports;
    {IocpReceiver receiver(socket());require(receiver.post(99,4).accepted(),"pending");retained=last_port();retained->broken=true;}
    require(closed_ports==closed&&retained->accepted_uncollected&&!early_release,"broken port retained storage");
    // Cancellation has queued a packet; its buffer still points at retained State.
    retained->buffer[0]=42;
    std::puts("Broken completion channel: storage intentionally retained until process exit");return 0;
  }
  {
    IocpReceiver receiver(socket());
    require(!receiver.post(0,1).accepted()&&!receiver.post(1,0).accepted()&&!receiver.post(1,17).accepted(),"bounds");
    mode=SubmitMode::rejected;auto rejected=receiver.post(1,2);require(!rejected.accepted()&&rejected.error==WSAECONNRESET,"submission failure no packet");
    mode=SubmitMode::immediate;require(receiver.post(2,2).state==Submission::immediate,"immediate success");
    require(!receiver.take()&&!receiver.post(3,2).accepted(),"immediate success still uncollected");
    require(receiver.poll(0).state==PacketState::result,"dequeue immediate packet");
    require(!receiver.post(3,2).accepted(),"ready result must be taken");
    auto first=receiver.take();require(first&&first->request==2&&first->count==1&&first->bytes[0]==11,"owned result");
    require(!receiver.take(),"take once");
    mode=SubmitMode::pending;require(receiver.post(3,16).state==Submission::pending,"pending accepted");
    require(receiver.poll(0).state==PacketState::timeout&&!receiver.take(),"timeout is not completion");
    require(receiver.wake()&&receiver.poll(0).state==PacketState::woken,"wake is not receive completion");
    require(!receiver.post(4,2).accepted(),"wake does not release operation");
    finish(*last_port(),TRUE,2,0);
    require(receiver.request_cancel().state==CancelState::not_found,"completion already queued");
    require(receiver.poll(0).state==PacketState::result,"success survives late cancel");
    auto late=receiver.take();require(late&&late->kind==CompletionKind::data&&late->count==2&&first->count==1,"result value ownership");
    require(receiver.post(4,2).accepted(),"submit cancellable");
    require(receiver.request_cancel().state==CancelState::requested&&!receiver.take(),"cancel request not collected");
    require(!receiver.post(5,2).accepted(),"cancel does not release buffer");
    require(receiver.poll(0).state==PacketState::result,"FALSE/non-null operation is completion");
    auto cancelled=receiver.take();require(cancelled&&cancelled->kind==CompletionKind::cancelled&&cancelled->count==0,"cancelled result");
    require(receiver.post(5,2).accepted(),"submit EOF");finish(*last_port(),TRUE,0,0);
    receiver.poll(0);require(receiver.take()->kind==CompletionKind::eof,"positive-length zero result means EOF");
    require(receiver.post(6,2).accepted(),"submit failed completion");finish(*last_port(),FALSE,1,WSAECONNRESET);
    receiver.poll(0);auto failed=receiver.take();require(failed->kind==CompletionKind::error&&failed->count==0&&failed->error==WSAECONNRESET,"failed bytes not parsed");
    require(receiver.post(7,2).accepted(),"submit malformed count");finish(*last_port(),TRUE,3,0);
    receiver.poll(0);require(receiver.take()->error==ERROR_INVALID_DATA,"reject oversized count");
  }
  mode=SubmitMode::pending;
  {IocpReceiver receiver(socket());receiver.post(8,2);for(int i=0;i<70;++i)receiver.wake();}
  require(ports.empty()&&!early_release,"destructor collects cancellation after many wakes");
  mode=SubmitMode::immediate;
  {IocpReceiver receiver(socket());receiver.post(9,2);}
  require(ports.empty()&&!early_release,"destructor collects already-successful packet");
  require(closed_ports==closed_sockets,"one cleanup per receiver");
  std::puts("IOCP source with API doubles: immediate/pending/rejection, packet identity, timeout/wake, late cancel, failed completion, EOF, lifetime passed");
 }catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
