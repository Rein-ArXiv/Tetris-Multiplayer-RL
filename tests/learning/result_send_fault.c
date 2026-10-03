// Linux-only test relay shim. Real TCP bytes, deterministic partial/EAGAIN/error.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static ssize_t (*real_send)(int,const void*,size_t,int);
static int states[65536],failed_first;
static long long unblock_ns[65536];
static long long now_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (long long)t.tv_sec*1000000000+t.tv_nsec;}
ssize_t send(int fd,const void* data,size_t count,int flags){
    pthread_mutex_lock(&lock);
    if(!real_send)real_send=dlsym(RTLD_NEXT,"send");
    const unsigned char* bytes=data;const int tracked=fd>=0 && fd<65536;
    const char* mode=getenv("TETRIS_TEST_SEND_MODE");if(!mode)mode="short";
    if(tracked && (states[fd]==3 || (states[fd]==4 && now_ns()<unblock_ns[fd]))){
        pthread_mutex_unlock(&lock);errno=EAGAIN;return -1;
    }
    if(tracked && states[fd]==1){states[fd]=2;pthread_mutex_unlock(&lock);errno=EAGAIN;return -1;}
    if(tracked && states[fd]==4)states[fd]=2;
    const int result_frame=count>=3 && bytes[0]==14 && bytes[1]==0 && bytes[2]==19;
    const int marker=count>=11 && bytes[0]==9 && bytes[1]==0 && bytes[2]==6 && memcmp(bytes+3,"fifo145!",8)==0;
    if(tracked && states[fd]==0 && strcmp(mode,"queue")==0 && marker){
        const ssize_t result=real_send(fd,data,3,flags);
        if(result==3){states[fd]=4;unblock_ns[fd]=now_ns()+300000000;}
        pthread_mutex_unlock(&lock);return result;
    }
    if(tracked && states[fd]==0 && result_frame && strcmp(mode,"queue")!=0){
        if(strcmp(mode,"blocked")==0){states[fd]=3;pthread_mutex_unlock(&lock);errno=EAGAIN;return -1;}
        if(strcmp(mode,"fail-first")==0 && !failed_first){failed_first=1;states[fd]=2;pthread_mutex_unlock(&lock);errno=ECONNRESET;return -1;}
        const ssize_t result=real_send(fd,data,3,flags);
        if(result==3)states[fd]=1;
        pthread_mutex_unlock(&lock);return result;
    }
    const ssize_t result=real_send(fd,data,count,flags);
    pthread_mutex_unlock(&lock);return result;
}
