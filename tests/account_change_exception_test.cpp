#include "meta/database.h"
#include "meta/credentials.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

// Dedicated test target: compile the real digest under another name so that
// the database's normal symbol can inject one exception without production hooks.
#define digest digest_without_fault
#include "../meta/credentials.cpp"
#undef digest
namespace {
int digest_calls = 0;
int fail_on = 0;
}
namespace meta::credentials {
std::string digest(const std::string& purpose, const std::string& value) {
    if (++digest_calls == fail_on) throw std::runtime_error("injected digest failure");
    return digest_without_fault(purpose,value);
}
}
static void require(bool value) {if(!value)throw std::runtime_error("account exception contract");}
int main(int argc,char** argv) {
    try {
        require(argc==2 && !std::filesystem::exists(argv[1]));
        meta::Database db(argv[1]);
        const std::string old(32,'a'),next(32,'b'),recovery="rc1."+std::string(64,'c');
        const auto player=db.registerGuest(old);require(player.has_value());
        std::optional<meta::Player> result;
        digest_calls=0;fail_on=5; // Four request digests, then profile lookup after UPDATE.
        bool threw=false;
        try { db.changeAccount("rotate",old,next,recovery,result); }
        catch (const std::runtime_error&) {threw=true;}
        fail_on=0;require(threw);
        const bool old_valid=db.getByToken(old).has_value();
        const bool new_valid=db.getByToken(next).has_value();
        std::cout<<"after exception: old="<<old_valid<<" new="<<new_valid<<'\n';
        require(old_valid && !new_valid);
        require(db.changeAccount("rotate",old,next,recovery,result)==meta::AccountChangeResult::Ok);
        require(result && result->auth_epoch==1 && result->id==player->id);
        std::cout<<"exception rollback, unlocked retry and epoch passed\n";
        return 0;
    } catch(const std::exception&) {std::cerr<<"account exception contract failed\n";return 1;}
}
