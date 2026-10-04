#include "settings/session.h"
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#define CHECK(e) do{if(!(e)){std::fprintf(stderr,"settings line %d: %s\n",__LINE__,#e);std::exit(1);}}while(false)
using namespace study_settings;
static void put(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;f.close();CHECK(f);}
static std::string get(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char** argv){
 CHECK(argc==2);const std::filesystem::path dir=argv[1];std::filesystem::remove_all(dir);std::filesystem::create_directories(dir);
 const auto good=parse(" # configuration\r\n character = rook # canonical\r\n decorations=0\nversion=1");CHECK(good&&!good.config.decorations&&good.config.character=="rook");
 CHECK(parse("version=1\n").config.decorations&&parse("version=1").config.character=="player");
 CHECK(*encode(good.config)=="version=1\ndecorations=0\ncharacter=rook\n");
 struct Bad {const char* input;ParseError error;std::size_t line;};
 const Bad cases[]={
 {"",ParseError::missing_version,1},{"#comment\n",ParseError::missing_version,1},
 {"version=2\n",ParseError::unsupported_version,1},{"version=oops",ParseError::invalid_value,1},
 {"version=1\ndecorations=1x",ParseError::invalid_value,2},
 {"version=1\ncharacter=removed",ParseError::invalid_value,2},
 {"version=1\ncharacter=rook\ncharacter=player",ParseError::duplicate_key,3},
 {"version=1\nversion=1",ParseError::duplicate_key,2},
 {"version=1\ndecorations=0\ndecorations=1",ParseError::duplicate_key,3},
 {"version=1\nunknown=0",ParseError::unknown_key,2},
 {"version=1\nnoequals",ParseError::malformed,2},
 {"version=1\ncharacter=rook=bad",ParseError::malformed,2},
 {"version=1\n=rook",ParseError::malformed,2}
 };
 for(const auto& c:cases){const auto result=parse(c.input);CHECK(!result&&result.error==c.error&&result.line==c.line&&result.config.decorations&&result.config.character=="player");}
 const auto late=parse("version=1\ndecorations=0\ncharacter=rook\nbroken");CHECK(!late&&late.config.decorations&&late.config.character=="player");
 CHECK(parse("version=1\n"+std::string(256,'#')));
 CHECK(parse("version=1\n"+std::string(257,'#')).error==ParseError::line_too_long);
 std::string limit="version=1\n";while(limit.size()+2<=4096)limit+="#\n";CHECK(limit.size()==4096&&parse(limit));CHECK(parse(limit+"x").error==ParseError::too_large);
 std::string nul="version=1\ncharacter=rook";nul.push_back('\0');nul+='x';CHECK(parse(nul).error==ParseError::malformed);
 auto owned=parse(std::string("version=1\ncharacter=rook"));CHECK(owned.config.character=="rook");
 for(bool decorations:{false,true})for(const auto& c:study_character_art::characters){Config original{decorations,std::string(c.id)};const auto encoded=encode(original);CHECK(encoded);const auto restored=parse(*encoded);CHECK(restored&&restored.config.decorations==decorations&&restored.config.character==c.id);}
 CHECK(!encode({true,"missing"}));
 const auto target=dir/"nested/settings.cfg";CHECK(load(target).status==LoadStatus::missing&&load(target).writable());
 Session first(target);CHECK(first.apply(study_menu::Action::none)==Change::unchanged&&!std::filesystem::exists(target));
 CHECK(first.apply(study_menu::Action::next_badge)==Change::saved);CHECK(first.apply(study_menu::Action::toggle_decorations)==Change::saved);
 Session restarted(target);CHECK(restarted.preferences().character_id()=="rook"&&!restarted.preferences().decorations());
 const auto stored=get(target);CHECK(!save(target,{true,"missing"})&&get(target)==stored);
 CHECK(!save({},Config{})&&!save(dir/"",Config{}));
 for(const std::string& bad:{std::string("version=99\ncharacter=rook\n"),std::string("version=1\nBROKEN"),std::string(4097,'x')}){
  put(target,bad);Session blocked(target);CHECK(blocked.initial_load().status==LoadStatus::invalid&&!blocked.initial_load().writable());
  CHECK(blocked.apply(study_menu::Action::toggle_decorations)==Change::blocked&&!blocked.preferences().decorations()&&get(target)==bad);
  std::filesystem::rename(target,target.string()+".bak");Session recovered(target);CHECK(recovered.initial_load().status==LoadStatus::missing&&recovered.preferences().decorations());CHECK(recovered.apply(study_menu::Action::next_badge)==Change::saved);CHECK(get(target.string()+".bak")==bad);std::filesystem::remove(target.string()+".bak");
 }
 Session directory(dir);CHECK(directory.initial_load().status==LoadStatus::io_error&&directory.apply(study_menu::Action::next_badge)==Change::blocked);
 const auto conflict=dir/"later.cfg";Session failure(conflict);std::filesystem::create_directory(conflict);
 CHECK(failure.apply(study_menu::Action::next_badge)==Change::failed&&failure.preferences().character_id()=="rook"&&std::filesystem::is_directory(conflict));
 for(const auto& entry:std::filesystem::directory_iterator(dir))CHECK(entry.path().filename().string().find(".tmp-")==std::string::npos);
 std::printf("Settings: strict parse/limits/rollback/owned IDs/4 roundtrips; actual create/replace/restart/protected-invalid/backup recovery/failed-save session passed\n");
}
