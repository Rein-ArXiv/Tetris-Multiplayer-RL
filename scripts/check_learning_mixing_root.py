"""Actual root SDL mixing body and Windows volume setters; no hardware claim."""
from pathlib import Path
import os,shlex,subprocess
from check_learning_text_layout import run
from check_learning_utf8 import cut
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'out/learning-checkpoints/78-mixing-check/root'
POST=r'''
#include <array>
#include <algorithm>
#include <limits>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"root mix %d: %s\n",__LINE__,#x);exit(1);}}while(false)
static void setup(const std::array<int,3>&order){
 s_initialized=true;s_have.channels=2;s_have.freq=44100;s_sounds.clear();s_sounds.resize(4);s_bgm={};
 for(auto&v:s_sfx)v={};const int values[3]={30000,30000,-30000};
 for(int i=0;i<3;++i){auto&d=s_sounds[i+1];d.pcm.assign(1200,static_cast<int16_t>(values[order[i]]));d.channels=2;d.sampleRate=44100;d.valid=true;s_sfx[i]={i+1,0,false,true};}
}
int main(int argc,char**){
 std::array<int,3> order{0,1,2};
 if(argc>1){setup(order);audio_set_sfx_volume(std::numeric_limits<float>::quiet_NaN());int16_t out[2]={9,9};audio_callback(nullptr,reinterpret_cast<Uint8*>(out),4);CHECK(out[0]==0&&out[1]==0&&s_sfx[0].pos==2);puts("NaN normalized before float-to-int; silent voices advance.");return 0;}
 bool saw_lost=false,saw_full=false;
 do{setup(order);int16_t out[2]{};audio_callback(nullptr,reinterpret_cast<Uint8*>(out),4);CHECK(out[0]==out[1]);saw_lost|=out[0]==2767;saw_full|=out[0]==30000;if(!LEGACY)CHECK(out[0]==30000);}while(std::next_permutation(order.begin(),order.end()));
 CHECK(saw_full);CHECK(saw_lost==LEGACY);
 if(!LEGACY){
  setup({0,1,2});alignas(int16_t) std::array<unsigned char,2056> bytes;bytes.fill(0xAA);audio_callback(nullptr,bytes.data()+1,2053);
  for(int i=0;i<513*2;++i){int16_t got;memcpy(&got,bytes.data()+1+i*2,2);CHECK(got==30000);}CHECK(bytes[0]==0xAA&&bytes[2053]==0&&bytes[2054]==0xAA);
  for(float g:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-1.0f,0.5f,2.0f}){
   audio_set_music_volume(g);audio_set_sfx_volume(g);const float want=!std::isfinite(g)||g<0?0:g>1?1:g;CHECK(s_musicVol==want&&s_sfxVol==want);
  }
 }
 puts(LEGACY?"Before: voice order changes 30000 to 2767.":"After: all permutations 30000; 513-frame chunk/unaligned/tail and finite volume policy passed.");
}
'''
def main():
 OUT.mkdir(parents=True,exist_ok=True);src=(ROOT/'audio/sdl_audio.cpp').read_text();legacy=(ROOT/'tests/learning/legacy_mix_s16.inc').read_text();before=src
 for symbol in ['static void mix_voice(','static void SDLCALL audio_callback(']:before=before.replace(cut(before,symbol),cut(legacy,symbol))
 before=before.replace('    v01 = audio_mix::normalize_gain(v01);','    if(v01<0)v01=0;\n    if(v01>1)v01=1;')
 flags=shlex.split(run(['pkg-config','--cflags','--libs','sdl2']).stdout)
 for label,text in [('before',before),('after',src)]:
  p=OUT/(label+'.cpp');p.write_text(text+POST.replace('LEGACY','true' if label=='before' else 'false'));exe=OUT/label
  run(['c++','-std=c++17','-O1','-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-I'+str(ROOT),'-I'+str(ROOT/'audio'),str(p),str(ROOT/'audio/mp3_decode.cpp'),*flags,'-pthread','-o',str(exe)])
  print(run([str(exe)]).stdout.strip(),flush=True)
  r=subprocess.run([str(exe),'nan'],capture_output=True,text=True,cwd=ROOT)
  (OUT/(label+'-nan.log')).write_text(r.stdout+r.stderr)
  if label=='before':assert r.returncode!=0 and 'runtime error' in r.stderr and 'nan' in r.stderr.lower();print('Before: NaN float-to-integer conversion rejected by sanitizer.',flush=True)
  else:assert r.returncode==0,(r.stdout,r.stderr);print(r.stdout.strip(),flush=True)
 win=(ROOT/'audio/audio.cpp').read_text()
 body='\n'.join(cut(win,s) for s in ['void audio_set_music_volume(','void audio_set_sfx_volume('])
 p=OUT/'windows-volume.cpp';p.write_text('#include "audio/mix_s16.h"\n#include <limits>\n#include <cstdio>\nstatic float s_musicVol=1,s_sfxVol=1;struct Voice{float volume=1;void SetVolume(float v){volume=v;}};static Voice voice;static Voice*s_musicVoice=&voice;\n'+body+r'''
int main(){for(float g:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-1.f,0.f,0.5f,2.f}){audio_set_music_volume(g);audio_set_sfx_volume(g);float want=!std::isfinite(g)||g<0?0:g>1?1:g;if(s_musicVol!=want||s_sfxVol!=want||voice.volume!=want)return 1;}std::puts("Windows actual setter bodies: finite policy passed; native XAudio2 not run.");}
''');exe=OUT/'windows-volume';run(['c++','-std=c++17','-include','initializer_list','-I'+str(ROOT),str(p),'-o',str(exe)]);print(run([str(exe)]).stdout.strip(),flush=True)
if __name__=='__main__':main()
