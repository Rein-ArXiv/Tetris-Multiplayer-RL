#include "client/labels.h"
#include "client/application.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"CHECK failed %d\n",__LINE__);std::exit(1);}}while(false)
int main(){
 const auto labels=study_labels::make();CHECK(labels&&labels->menu.count==3&&labels->play.count==3);
 CHECK(study_labels::menu_text.size()==9&&labels->menu.scalars[0]==0xB300&&labels->menu.scalars[1]==0xAE30&&labels->menu.scalars[2]==0xC2E4);
 CHECK(labels->play.scalars[0]==0xD50C&&labels->play.scalars[1]==0xB808&&labels->play.scalars[2]==0xC774);
 CHECK(study_labels::decode("")->count==0);
 CHECK(study_labels::decode("1234567890123456")->count==16);CHECK(!study_labels::decode("12345678901234567"));
 auto bad=study_labels::decode(std::string_view("\xFF\0X",3));CHECK(bad&&bad->count==3&&bad->invalid_bytes==1&&bad->scalars[1]==0&&bad->scalars[2]==U'X');
 CHECK(study_labels::decode(u8"가")->count==1&&study_labels::decode(u8"\u1100\u1161")->count==2);
 auto round=study_round::Round::create_seeded(study_grid::Grid{},1);CHECK(round);
 study_app::Application app(*round);CHECK(app.screen()==study_app::Screen::menu);
 const study_labels::Label* current=&labels->menu;
 CHECK(app.advance(0,{},true,false,false));CHECK(app.screen()==study_app::Screen::playing);current=&labels->play;CHECK(current->scalars[0]==0xD50C);
 CHECK(app.advance(0,{},false,true,true));CHECK(app.screen()==study_app::Screen::menu);current=&labels->menu;CHECK(current->scalars[0]==0xB300);
 std::puts("Labels: capacity/invalid bytes, decomposed Hangul scalars, menu/play transitions passed");
}
