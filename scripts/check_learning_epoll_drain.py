"""Actual production poll body: a busy wake producer must not extend draining indefinitely."""
from pathlib import Path
import sys
from check_learning_text_layout import run
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'out/learning-checkpoints/127-root-drain-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 s=(ROOT/'net/reactor_epoll.cpp').read_text();a=s.index('    int poll(');b=s.index('\n    // 이전 배치',a);body=s[a:b]
 before='--before'in sys.argv
 if before:body=(ROOT/'out/learning-jobs/127-poll-before.txt').read_text()
 head=r'''#include <sys/epoll.h>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <vector>
int reads=0;bool interrupt=false;void* marker=nullptr;
int epoll_wait(int,epoll_event* events,int,int){events[0]={};events[0].data.ptr=marker;events[0].events=EPOLLIN;return 1;}
ssize_t read(int,void*,size_t size){++reads;if(interrupt&&reads==1){errno=EINTR;return -1;}if(reads<=100)return size;errno=EAGAIN;return -1;}
struct Event{void*token=nullptr;bool readable=false,writable=false,error=false;};
struct Base{virtual ~Base()=default;virtual int poll(std::vector<Event>&,int)=0;};
struct Probe:Base{int epfd_=5,wakefd_=6;char wake_marker_=0;std::vector<epoll_event>scratch_;
'''
 tail='''};int main(){Probe p;marker=&p.wake_marker_;std::vector<Event>out;'''
 if before:tail+='''if(p.poll(out,0)!=0||reads!=101)return 1;reads=0;interrupt=true;p.poll(out,0);if(reads!=1)return 2;std::puts("Before: follows 100 producer refills and fails to retry interrupted read");}'''
 else:tail+='''if(p.poll(out,0)!=0||reads!=1)return 1;reads=0;interrupt=true;p.poll(out,0);if(reads!=2)return 2;std::puts("After: one successful counter read per batch; EINTR retries; later refills remain for next poll");}'''
 f=OUT/('before.cpp'if before else'after.cpp');f.write_text(head+body+tail);exe=f.with_suffix('')
 run(['c++','-std=c++17','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(f),'-o',str(exe)],timeout=30)
 print(run([str(exe)],timeout=5).stdout,flush=True)
if __name__=='__main__':main()
