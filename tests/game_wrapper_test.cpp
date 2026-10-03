// Link the real Game with deterministic audio/draw probes, without an OS device.
#include "src/game.h"
#include "src/presentation.h"
#include "renderer/renderer.h"
#include <set>
#include <vector>
#include <type_traits>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
namespace {
int references=0,next_handle=1,duplicate_unloads=0,stops=0,draws=0;
bool init_ok=true,missing_drop=false,fail_all_loads=false,fail_play=false;
int failed_plays=0;
std::set<int> live;
std::vector<int> plays;
}
bool audio_init(){++references;return init_ok;}
void audio_shutdown(){--references;}
AudioHandle audio_load_sound(const char* path){
    if(fail_all_loads)return 0;
    if(missing_drop&&std::string_view(path)=="Sounds/drop.mp3")return 0;
    int id=next_handle++;live.insert(id);return id;
}
void audio_unload_sound(AudioHandle h){if(h&&!live.erase(h))++duplicate_unloads;}
void audio_play_sound(AudioHandle h){if(h){CHECK(live.count(h));if(fail_play){++failed_plays;return;}plays.push_back(h);}}
void audio_play_music(AudioHandle h){CHECK(live.count(h));}
void audio_stop_music(){++stops;}
std::vector<Color> presentation_palette(std::vector<Color> colors){return colors;}
void draw_rect(int,int,int,int,Color){++draws;}
int main(){
#ifdef REPRODUCE_COPY
    {
        Game original(1);
        {
            Game copied=original;copied.score=9;
            CHECK(original.sim.Score()==9&&copied.sim.Score()==0);
        }
    }
    CHECK(duplicate_unloads==4&&references==-1);
    std::printf("before: copied alias changed original; duplicate unloads=%d refs=%d\n",duplicate_unloads,references);
#else
    static_assert(!std::is_copy_constructible_v<Game>);
    static_assert(!std::is_copy_assignable_v<Game>);
    static_assert(!std::is_move_constructible_v<Game>);
    static_assert(!std::is_move_assignable_v<Game>);
    {
        auto one=std::make_unique<Game>(1);
        CHECK(&one->score==&one->sim.score&&&one->gameOver==&one->sim.gameOver);
        CHECK(references==1&&live.size()==5);
        const auto* address=one.get();auto moved=std::move(one);
        CHECK(!one&&moved.get()==address&&&moved->score==&moved->sim.score);
        {
            Game other(2);CHECK(references==2&&live.size()==9);
            const auto hash=other.ComputeStateHash();other.Draw();other.DrawBoardAt(3,4);other.DrawNextAt(8,9);
            CHECK(draws>0&&other.ComputeStateHash()==hash);
            other.sim.rotateSoundEvent=true;other.SubmitInput(INPUT_NONE);
            CHECK(!other.sim.rotateSoundEvent&&!plays.empty());
            const auto count=plays.size();other.SubmitInput(INPUT_NONE);CHECK(plays.size()==count);
            other.sim.clearSoundEvent=true;other.sim.garbageSoundEvent=true;other.Tick();
            CHECK(!other.sim.clearSoundEvent&&!other.sim.garbageSoundEvent);
            // Hard drop can insert garbage inside SubmitInput, before Tick.
            other.sim.AddPendingGarbage(1);
            const auto before=plays.size();
            other.SubmitInput(INPUT_DROP);
            CHECK(plays.size()==before+2); // drop + inserted garbage
            CHECK(!other.sim.dropSoundEvent&&!other.sim.garbageSoundEvent);
            other.SubmitInput(INPUT_NONE);
            CHECK(plays.size()==before+2);
            // Every wrapper drains all categories, including direct down calls.
            other.sim.clearSoundEvent=true;
            const auto before_down=plays.size();
            other.MoveBlockDown();
            CHECK(!other.sim.clearSoundEvent&&plays.size()==before_down+1);
            const auto after_down=plays.size();
            other.Draw();other.Draw();CHECK(plays.size()==after_down);

        }
        CHECK(references==1&&live.size()==5&&stops==0);
    }
    CHECK(references==0&&live.empty()&&duplicate_unloads==0&&stops==1);
    init_ok=false;
    {Game failed(1);CHECK(references==1&&live.empty());
     SimGame reference(1);
     for(int i=0;i<120;++i){
         const auto input=static_cast<uint8_t>(i%17==0?INPUT_DROP:INPUT_NONE);
         failed.SubmitInput(input);reference.SubmitInput(input);
         failed.Tick();reference.Tick();
         CHECK(failed.ComputeStateHash()==reference.StateHash());
         CHECK(!failed.sim.rotateSoundEvent&&!failed.sim.dropSoundEvent&&
               !failed.sim.clearSoundEvent&&!failed.sim.garbageSoundEvent);
     }
    }
    CHECK(references==0&&live.empty());init_ok=true;missing_drop=true;
    {
        Game fallback(1);const int rotate=next_handle-4; // rotate, clear, garbage, music allocated.
        fallback.sim.dropSoundEvent=true;fallback.SubmitInput(INPUT_NONE);
        CHECK(plays.back()==rotate&&!fallback.sim.dropSoundEvent);
    }
    CHECK(references==0&&live.empty()&&duplicate_unloads==0);
    missing_drop=false;
    // Same seed/input sequence: presentation failure cannot change rule hashes.
    for(int mode=0;mode<3;++mode){
        init_ok=mode!=0;fail_all_loads=mode==1;fail_play=mode==2;
        for(unsigned seed=0;seed<24;++seed){
            Game game(seed);SimGame oracle(seed);
            for(unsigned tick=0;tick<240;++tick){
                const auto mask=static_cast<uint8_t>(
                    (tick%13==0?INPUT_DROP:0)|(tick%7==0?INPUT_ROTATE:0)|
                    (tick%3==0?INPUT_LEFT:0)|(tick%5==0?INPUT_RIGHT:0));
                game.SubmitInput(mask);oracle.SubmitInput(mask);
                game.Tick();oracle.Tick();
                CHECK(game.ComputeStateHash()==oracle.StateHash());
                CHECK(!game.sim.rotateSoundEvent&&!game.sim.dropSoundEvent&&
                      !game.sim.clearSoundEvent&&!game.sim.garbageSoundEvent);
            }
        }
        CHECK(references==0&&live.empty()&&duplicate_unloads==0);
    }
    CHECK(failed_plays>0);init_ok=true;fail_all_loads=fail_play=false;
    std::puts("Game: stable aliases, unique_ptr transfer, shared music, event consumption, failed init, fallback and one release per handle passed.");
#endif
}
