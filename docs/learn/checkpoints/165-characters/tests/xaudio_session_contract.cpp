#include "presentation/sound_session.h"
#include <cstdio>
using namespace fake_xaudio;
int main() {
    for (const auto failure : {Fail::com, Fail::engine, Fail::master}) {
        reset(); fail=failure;
        { study_sound::Session s; require(!s.prepare(), "failed prepare");
          require(!s.play(study_sound::Kind::drop), "unprepared play"); s.stop(); }
        require(com_refs==0&&engines==0&&masters==0&&sources==0,"failed session cleanup");
    }
    reset();
    { study_sound::Session s;
      require(!s.play(study_sound::Kind::rotate),"play before prepare");
      require(s.prepare()&&s.prepare(),"idempotent prepare");
      require(inits==1,"prepare must not reopen");
      for(int i=0;i<12;++i) require(s.play(static_cast<study_sound::Kind>(i%4)),"cue dispatch");
      require(sources==9,"bounded pool"); read_all();
      require(!s.play(static_cast<study_sound::Kind>(4)),"invalid kind");
      s.stop(); s.stop(); require(sources==0,"stop all");
      for(const auto failure : {Fail::source,Fail::submit,Fail::start}) {
          fail=failure;require(!s.play(study_sound::Kind::drop),"play failure");
          require(sources==0,"failed voice cleaned");
      }
      fail=Fail::none;require(s.play(study_sound::Kind::drop),"retry after failure");
    }
    require(com_refs==0&&engines==0&&masters==0&&sources==0,"session destruction");
    reset();std::puts("Real Session + XAudio backend: four cues, bounded overlap, idempotence, failure cleanup passed (API double).");
}
