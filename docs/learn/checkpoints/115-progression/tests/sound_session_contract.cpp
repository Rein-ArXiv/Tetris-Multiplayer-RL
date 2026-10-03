#include "presentation/sound_session.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
int main(int argc,char**){
    study_sound::Session s;
    if(argc>1){
        CHECK(!s.prepare());CHECK(!s.prepare());
        CHECK(!s.play(study_sound::Kind::rotate));s.stop();
#ifdef STUDY_AUDIO_SDL
        CHECK(SDL_WasInit(SDL_INIT_AUDIO)==0);
#endif
        std::puts("Unavailable device: no active audio subsystem or pending retry.");
        return 0;
    }
    CHECK(!s.play(study_sound::Kind::rotate));
    CHECK(s.prepare()==study_sound::Session::supported);
    CHECK(s.prepare()==study_sound::Session::supported);
    for(int i=0;i<4;++i)CHECK(s.play(static_cast<study_sound::Kind>(i))==study_sound::Session::supported);
    CHECK(!s.play(static_cast<study_sound::Kind>(-1)));
    CHECK(!s.play(static_cast<study_sound::Kind>(4)));
    s.stop();s.stop();
    CHECK(s.play(study_sound::Kind::drop)==study_sound::Session::supported);
    std::puts("Session: prepared cue dispatch, repeated prepare/stop and invalid kinds passed.");
}
