"""Linux fault injection at the real temporary-file publication boundary."""
from pathlib import Path
import os,subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/73-settings-check'
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 # The teaching copy uses the actual writer, with only its namespace changed.
 assert cut((ROOT/'meta/private_file.cpp').read_text(),'bool write_private_file(')==cut((ROOT/'docs/learn/checkpoints/73-settings/settings/private_file.cpp').read_text(),'bool write_private_file(')
 source=OUT/'file-fault.c';source.write_text(r'''#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
int fsync(int fd){
 static int (*real_sync)(int);if(!real_sync)real_sync=dlsym(RTLD_NEXT,"fsync");
 const char* mode=getenv("STUDY_FILE_FAULT");struct stat info;
 if(mode&&fstat(fd,&info)==0&&((strcmp(mode,"file-sync")==0&&S_ISREG(info.st_mode))||(strcmp(mode,"directory-sync")==0&&S_ISDIR(info.st_mode)))){errno=EIO;return -1;}
 return real_sync(fd);
}
int rename(const char* from,const char* to){
 static int (*real_rename)(const char*,const char*);if(!real_rename)real_rename=dlsym(RTLD_NEXT,"rename");
 const char* mode=getenv("STUDY_FILE_FAULT");if(mode&&strcmp(mode,"rename")==0){errno=EACCES;return -1;}return real_rename(from,to);
}
''')
 library=OUT/'file-fault.so';run(['cc','-shared','-fPIC','-O1',str(source),'-ldl','-o',str(library)])
 binary=OUT/'root-settings-after'
 for stage in ['file-sync','rename','directory-sync']:
  directory=OUT/('fault-'+stage);directory.mkdir(exist_ok=True);target=directory/'settings.cfg';target.write_text('previous settings\n')
  result=subprocess.run([str(binary),'save',str(target)],cwd=ROOT,env={**os.environ,'LD_PRELOAD':str(library),'STUDY_FILE_FAULT':stage},capture_output=True,text=True)
  assert result.returncode==2,(stage,result.stdout,result.stderr)
  text=target.read_text();assert (text.startswith('bgm_vol=37\n') if stage=='directory-sync' else text=='previous settings\n')
  assert list(directory.iterdir())==[target]
  (OUT/('fault-'+stage+'.log')).write_text(result.stdout+result.stderr)
  print(stage+': false; '+('new complete contents visible' if stage=='directory-sync' else 'old bytes preserved')+'; temp cleaned',flush=True)
if __name__=='__main__':main()
