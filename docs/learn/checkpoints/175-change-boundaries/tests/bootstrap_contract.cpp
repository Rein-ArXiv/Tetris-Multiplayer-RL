#include "meta/account_bootstrap.h"
#include <iostream>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"failed line "<<__LINE__<<": " #x "\n";std::exit(1);}}while(0)
using namespace study_meta;
const IssuedAccount account{42,std::string(32,'a')};
struct FakeStore {
    std::string origin_="http://127.0.0.1:18082";
    SavedAccount value{AccountLoad::missing,{}};
    bool unlocked=true, fail_save=false;int saves=0;
    bool available()const{return unlocked;}
    const std::string& origin()const{return origin_;}
    SavedAccount load()const{return value;}
    bool save(const IssuedAccount& x){++saves;if(fail_save)return false;value={AccountLoad::ready,x};return true;}
};
struct FakeApi {
    std::string origin_="http://127.0.0.1:18082";
    int issues=0,verifies=0;bool lost_issue=false;
    VerifiedProfile result{VerifyState::accepted,PublicProfile{42,0,0,0}};
    bool valid()const{return true;}
    const std::string& origin()const{return origin_;}
    std::optional<IssuedAccount> issue(){++issues;return lost_issue ? std::nullopt : std::optional<IssuedAccount>{account};}
    VerifiedProfile verify(const std::string& token){CHECK(token==account.token);++verifies;return result;}
};
void states() {
    FakeStore store;FakeApi api;AccountBootstrap session;
    store.fail_save=true;
    auto result=session.connect(store,api);
    CHECK(result.state==BootstrapState::unsaved && !result.profile);
    CHECK(api.issues==1 && api.verifies==0 && session.has_unsaved_key());
    CHECK(session.connect(store,api).state==BootstrapState::unsaved && api.issues==1);
    FakeStore other;FakeApi other_api;other.origin_="http://127.0.0.1:18083";other_api.origin_=other.origin_;
    CHECK(session.connect(other,other_api).state==BootstrapState::storage_blocked);
    CHECK(other.saves==0 && other_api.issues==0 && other_api.verifies==0 && session.has_unsaved_key());
    store.fail_save=false;
    CHECK(session.connect(store,api).state==BootstrapState::online);
    CHECK(api.issues==1 && api.verifies==1 && !session.has_unsaved_key() && store.value.account.token==account.token);
    AccountBootstrap restart;
    api.result={VerifyState::unavailable,{}};
    CHECK(restart.connect(store,api).state==BootstrapState::offline_saved && api.issues==1);
    api.result={VerifyState::rejected,{}};
    CHECK(restart.connect(store,api).state==BootstrapState::key_rejected && api.issues==1);
    CHECK(store.value.account.token==account.token);
    api.result={VerifyState::accepted,PublicProfile{43,0,0,0}};
    CHECK(restart.connect(store,api).state==BootstrapState::offline_saved);
    for(auto state:{AccountLoad::invalid,AccountLoad::io_error,AccountLoad::wrong_origin}) {
        FakeStore broken;broken.value.status=state;FakeApi untouched;AccountBootstrap b;
        CHECK(b.connect(broken,untouched).state==BootstrapState::storage_blocked);
        CHECK(untouched.issues==0 && untouched.verifies==0 && broken.saves==0);
    }
    FakeStore busy;busy.unlocked=false;FakeApi untouched;AccountBootstrap b;
    CHECK(b.connect(busy,untouched).state==BootstrapState::storage_blocked && untouched.issues==0);
    FakeStore fresh;FakeApi lost;lost.lost_issue=true;AccountBootstrap uncertain;
    CHECK(uncertain.connect(fresh,lost).state==BootstrapState::creation_unconfirmed);
    CHECK(uncertain.connect(fresh,lost).state==BootstrapState::creation_unconfirmed && lost.issues==1);
    CHECK(!uncertain.has_unsaved_key() && fresh.saves==0);
}
bool fail_before(const std::string&,const std::string&){return false;}
bool fail_after(const std::string& path,const std::string& body){
    CHECK(study_files::write_private_file(path,body));return false;
}
void files() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("study-bootstrap-"+new_identity().token);
    CHECK(fs::create_directory(root));
    struct Cleanup{fs::path p;~Cleanup(){std::error_code e;fs::remove_all(p,e);}} cleanup{root};
    const std::string origin="http://127.0.0.1:18082";
    {
        LocalAccountStore store(root,origin);CHECK(store.available() && store.load().status==AccountLoad::missing);
        LocalAccountStore contender(root,origin);CHECK(!contender.available());
        CHECK(store.save(account));CHECK(store.load().account.token==account.token);
        CHECK(!store.save({43,std::string(32,'b')}));CHECK(store.load().account.player_id==42);
#ifndef _WIN32
        CHECK((fs::status(root/"account.json").permissions() & (fs::perms::group_all|fs::perms::others_all))==fs::perms::none);
#endif
    }
    {LocalAccountStore reopened(root,origin);CHECK(reopened.available() && reopened.load().status==AccountLoad::ready);}
    {LocalAccountStore other(root,"http://127.0.0.1:18083");CHECK(other.load().status==AccountLoad::wrong_origin);CHECK(!other.save(account));}
    {std::ofstream f(root/"account.json");f<<"broken";}
    {LocalAccountStore broken(root,origin);CHECK(broken.load().status==AccountLoad::invalid);CHECK(!broken.save(account));}
    {std::ofstream f(root/"account.json");f<<std::string(kJsonBodyBytes+1,' ');}
    {LocalAccountStore oversized(root,origin);CHECK(oversized.load().status==AccountLoad::invalid);CHECK(!oversized.save(account));}
    fs::remove(root/"account.json");fs::create_directory(root/"account.json");
    {LocalAccountStore blocked(root,origin);CHECK(blocked.load().status==AccountLoad::io_error);CHECK(!blocked.save(account));}
    fs::remove(root/"account.json");
#ifndef _WIN32
    fs::create_symlink(root/"missing",root/"account.json");
    {LocalAccountStore link(root,origin);CHECK(link.load().status==AccountLoad::io_error);CHECK(!link.save(account));}
    fs::remove(root/"account.json");
#endif
    {LocalAccountStore before(root,origin,fail_before);CHECK(!before.save(account));CHECK(before.load().status==AccountLoad::missing);}
    {LocalAccountStore after(root,origin,fail_after);CHECK(!after.save(account));CHECK(after.load().status==AccountLoad::ready);}
    {LocalAccountStore retry(root,origin);CHECK(retry.save(account));CHECK(retry.load().account.token==account.token);}
    CHECK(parse_issued(Json{{"player_id",UINT64_MAX},{"token",account.token}}.dump())->player_id==UINT64_MAX);
    CHECK(!parse_issued(Json{{"player_id",0},{"token",account.token}}.dump()));
    CHECK(!parse_issued(Json{{"player_id",1},{"token","chosen"}}.dump()));
    CHECK(!parse_issued(Json{{"player_id",1},{"token",account.token},{"bp",0}}.dump()));
}
int main(){states();files();std::cout<<"Bootstrap: save/auth separation, same-key retry, origin binding, no replacement, lock/read errors and publication uncertainty passed\n";}
