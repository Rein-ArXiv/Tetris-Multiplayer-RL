#include "presentation/sound_session.h"
#include <chrono>
#include <thread>
#include <cstdio>
int main(){
 study_sound::Session audio;
 if(!audio.prepare()){std::fputs("Audio device unavailable\n",stderr);return 1;}
 for(auto kind:{study_sound::Kind::rotate,study_sound::Kind::drop,study_sound::Kind::clear,study_sound::Kind::garbage}){
  if(!audio.play(kind))return 1;
  std::this_thread::sleep_for(std::chrono::milliseconds(150));
 }
 audio.stop();std::puts("Four cue starts submitted; stop and destruction close the graph.");
}
