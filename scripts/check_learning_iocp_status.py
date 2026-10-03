"""Actual production IOCP completion-status block must use the documented result API."""
from pathlib import Path
import sys
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'out/learning-checkpoints/128-root-status-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 s=(ROOT/'net/reactor_iocp.cpp').read_text();a=s.index('            // OVERLAPPED_ENTRY::Internal');b=s.index('            out.push_back(ev);',a);block=s[a:b]
 before='--before'in sys.argv
 if before:block=(ROOT/'out/learning-jobs/128-status-before.txt').read_text()
 head=r'''#include <cstdint>
#include <cstdio>
using DWORD=std::uint32_t;using SOCKET=std::uintptr_t;
constexpr bool FALSE=false;struct OVERLAPPED{};struct State{SOCKET fd=9;OVERLAPPED ov;};
struct Entry{std::uintptr_t Internal;};struct Event{bool error=false;};
bool operation_ok=true;int calls=0;
bool WSAGetOverlappedResult(SOCKET fd,OVERLAPPED* ov,DWORD* count,bool wait,DWORD* flags){
 ++calls;if(fd!=9||!ov||!count||!flags||wait)return false;*count=0;*flags=0;return operation_ok;}
Event status(State* st,const Entry&e){Event ev;
'''
 tail='''return ev;}int main(){State state;operation_ok=true;auto a=status(&state,{123});operation_ok=false;auto b=status(&state,{0});'''
 if before:tail+='''if(!a.error||b.error||calls)return 1;std::puts("Before: reserved field overrides actual operation result");}'''
 else:tail+='''if(a.error||!b.error||calls!=2)return 1;std::puts("After: documented non-waiting operation query determines error, reserved bytes ignored");}'''
 f=OUT/('before.cpp'if before else'after.cpp');f.write_text(head+block+tail);exe=f.with_suffix('')
 run(['c++','-std=c++17','-fsanitize=undefined','-fno-sanitize-recover=all',str(f),'-o',str(exe)],timeout=30)
 print(run([str(exe)],timeout=5).stdout,flush=True)
if __name__=='__main__':main()
