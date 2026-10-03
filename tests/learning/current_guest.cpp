#include "meta/database.h"
#include "meta/sqlite_handle.h"
#include <filesystem>
#include <iostream>
using namespace study_meta;
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);CHECK(!std::filesystem::exists(argv[1]));meta::Database db(argv[1]);SqliteDb raw(argv[1]);
    raw.exec("CREATE TRIGGER reject_guest_icon BEFORE INSERT ON player_icons BEGIN SELECT RAISE(ABORT,'injected'); END;");
    const auto failed=db.registerGuest(std::string(32,'a'));
    Statement n(raw.get(),"SELECT count(*) FROM players");CHECK(n.next());
    const bool atomic=!failed && n.integer_column(0)==0;
    CHECK(!n.next());
    std::cout<<"ownership failure leaves no account: "<<atomic<<'\n';
    if(!atomic)return 1;
    raw.exec("DROP TRIGGER reject_guest_icon;");
    auto success=db.registerGuest(std::string(32,'a'));CHECK(success);
    CHECK(!db.registerGuest(std::string(32,'a')));
    CHECK(db.getByToken(std::string(32,'a'))->id==success->id);
    Statement count(raw.get(),"SELECT count(*) FROM players");CHECK(count.next() && count.integer_column(0)==1);
    Statement owned(raw.get(),"SELECT count(*) FROM player_icons");CHECK(owned.next() && owned.integer_column(0)==1);
    std::cout<<"successful account and default ownership, duplicate key rollback: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
