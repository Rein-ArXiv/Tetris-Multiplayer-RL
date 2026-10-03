#include "audio/player.h"
#include "audio/load_clip.h"
#include <cstdio>
#include <cstdlib>
#include <type_traits>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"playback line %d: %s: %s\n",__LINE__,#e,SDL_GetError());std::exit(1);}}while(false)
int main(int argc,char**argv){
    SDL_SetMainReady(); // Explicit console entry (SDL_MAIN_HANDLED).
 CHECK(argc==2);using namespace study_audio;using State=Player::State;
 static_assert(!std::is_copy_constructible_v<Player>&&!std::is_move_constructible_v<Player>);
 CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO)==0); // independently owned reference
 {
 Player p;CHECK(p.state()==State::closed&&!p.play());p.stop();p.unload();p.close();
 CHECK(!p.open(0,2)&&!p.open(44100,0));CHECK(p.open(48000,2));CHECK(!p.open(48000,2));CHECK(p.state()==State::empty&&!p.play());
 auto file=load_clip(argv[1]);CHECK(file);CHECK(p.replace(std::move(*file.pcm)));CHECK(p.state()==State::ready&&p.cursor_frames()==0);
 auto wrong=make_reference_tone();CHECK(!p.replace(std::move(wrong)));CHECK(wrong.layout().frames==44100&&p.state()==State::ready);
 CHECK(p.play());auto start=SDL_GetTicks();while(p.state()!=State::finished&&Uint32(SDL_GetTicks()-start)<2500)SDL_Delay(2);
 CHECK(p.state()==State::finished&&p.cursor_frames()==16128);p.stop();CHECK(p.state()==State::ready&&p.cursor_frames()==0);
 CHECK(p.play());p.unload();CHECK(p.state()==State::empty&&!p.play());p.unload();p.close();p.close();CHECK(p.state()==State::closed);
 CHECK(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO); // our close must not consume the other owner
 for(int i=0;i<12;++i){CHECK(p.open(44100,2));auto tone=make_reference_tone();CHECK(p.replace(std::move(tone)));CHECK(p.play());SDL_Delay(1);p.close();CHECK(p.state()==State::closed);}
 }
 CHECK(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO);SDL_QuitSubSystem(SDL_INIT_AUDIO);CHECK(!(SDL_WasInit(SDL_INIT_AUDIO)&SDL_INIT_AUDIO));
 std::puts("SDL dummy: lifecycle, format rejection, playback cursor, unload/reopen and subsystem ownership passed.");
}
