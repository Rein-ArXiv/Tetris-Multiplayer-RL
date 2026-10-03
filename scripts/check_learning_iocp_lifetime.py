"""Extract production destructor and exercise its lifetime under controlled completion delivery.
This is an API behavioral double, not Windows native IOCP execution.
"""
from pathlib import Path
import sys
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'out/learning-checkpoints/125-iocp-lifetime-check'
HEAD=r'''
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <vector>
using HANDLE=void*;using SOCKET=std::uintptr_t;using ULONG=unsigned long;using ULONG_PTR=std::uintptr_t;
struct Overlapped{};
struct OVERLAPPED_ENTRY{ULONG_PTR lpCompletionKey;Overlapped* lpOverlapped;};
constexpr unsigned INFINITE=~0u,WAIT_TIMEOUT=258;constexpr bool FALSE=false;
constexpr ULONG_PTR kWakeKey=1,kSockKey=2;
int early_free=0,cancels=0,closed=0,waits=0;unsigned last_error=0;
bool fail_port=false;int timeout_count=0,wake_count=0;
struct SockState{Overlapped ov;SOCKET fd=9;bool read_armed=false;bool kernel_pending=false;
 ~SockState(){if(kernel_pending)++early_free;}};
#define CONTAINING_RECORD(p,T,m) reinterpret_cast<T*>(reinterpret_cast<char*>(p)-offsetof(T,m))
std::vector<SockState*> pending_kernel;
bool CancelIoEx(HANDLE,Overlapped*){++cancels;return true;}
unsigned GetLastError(){return last_error;}
bool CloseHandle(HANDLE){++closed;return true;}
bool GetQueuedCompletionStatusEx(HANDLE,OVERLAPPED_ENTRY* batch,ULONG,ULONG* got,unsigned wait_ms,bool){
 ++waits;*got=0;
 if(fail_port){last_error=6;return false;}
 if(timeout_count>0){
  if(wait_ms!=INFINITE){--timeout_count;last_error=WAIT_TIMEOUT;return false;}
  timeout_count=0; // Model blocking through the delay to the next completion.
 }
 if(wake_count>0){--wake_count;batch[0]={kWakeKey,nullptr};*got=1;return true;}
 if(pending_kernel.empty()){last_error=WAIT_TIMEOUT;return false;}
 auto* st=pending_kernel.back();pending_kernel.pop_back();st->kernel_pending=false;
 batch[0]={kSockKey,&st->ov};*got=1;return true;
}
struct Base{virtual ~Base()=default;};
struct IocpReactor:Base{
 HANDLE iocp_=reinterpret_cast<HANDLE>(3);
 std::unordered_map<SOCKET,std::unique_ptr<SockState>> socks_;
 std::unordered_map<SockState*,std::unique_ptr<SockState>> zombies_;
'''
TAIL=r'''
 void add_pending(bool retired){auto p=std::make_unique<SockState>();p->fd=9+pending_kernel.size();p->read_armed=true;p->kernel_pending=true;
 auto* raw=p.get();pending_kernel.push_back(raw);if(retired)zombies_.emplace(raw,std::move(p));else socks_.emplace(raw->fd,std::move(p));}
};
void check(bool b,const char* text){if(!b){std::fprintf(stderr,"FAIL %s\n",text);std::exit(1);}}
void reset(){early_free=cancels=closed=waits=0;last_error=0;timeout_count=wake_count=0;fail_port=false;pending_kernel.clear();}
int main(){
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 s=(ROOT/'net/reactor_iocp.cpp').read_text();a=s.index('    ~IocpReactor() override {');b=s.index('\n    bool add(',a);method=s[a:b]
 before='--before' in sys.argv
 if before:method=(ROOT/'out/learning-jobs/125-iocp-destructor-before.txt').read_text()
 if before:
  checks=r'''reset();timeout_count=1;{IocpReactor r;r.add_pending(true);}check(early_free==1,"old timeout prematurely freed retired operation");pending_kernel.clear();
reset();{IocpReactor r;r.add_pending(false);}check(early_free==1&&cancels==0,"old registered operation not cancelled or drained");pending_kernel.clear();
reset();wake_count=70;{IocpReactor r;r.add_pending(true);}check(early_free==1&&waits==64,"old fixed drain cap abandoned queued completion");pending_kernel.clear();
std::puts("Before: timeout frees pending retired state; registered pending state is also freed");}'''
 else:
  checks=r'''reset();timeout_count=1;{IocpReactor r;r.add_pending(true);}check(early_free==0&&pending_kernel.empty()&&waits==1,"timeout followed by late completion retained storage");
reset();wake_count=70;{IocpReactor r;r.add_pending(false);r.add_pending(true);}check(early_free==0&&pending_kernel.empty()&&cancels==1&&waits==72,"more than64 wakes and registered operation drained");
reset();fail_port=true;{IocpReactor r;r.add_pending(false);r.add_pending(true);}check(early_free==0&&pending_kernel.size()==2&&closed==1,"broken port retains in-flight storage");
for(auto* p:pending_kernel){p->kernel_pending=false;delete p;}pending_kernel.clear();
reset();{IocpReactor r;}check(waits==0&&closed==1,"empty shutdown closes without waiting");
std::puts("After: timeout/70 wakes/registered reads drained; fatal-port storage retained; empty shutdown safe");}'''
 p=OUT/('before.cpp'if before else'after.cpp');p.write_text(HEAD+method+TAIL+checks)
 exe=p.with_suffix('')
 run(['c++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(p),'-o',str(exe)],timeout=60)
 print(run([str(exe)],timeout=10).stdout,flush=True)
if __name__=='__main__':main()
