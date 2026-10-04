#include "meta/sqlite_results.h"
#include "meta/account_crypto.h"
#include <filesystem>
#include <iostream>
using namespace study_meta;
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
static int unavailable(unsigned char*,int){return 0;}
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);CHECK(!std::filesystem::exists(argv[1]));
    bool failed=false;try{new_identity(unavailable);}catch(const std::runtime_error&){failed=true;}CHECK(failed);
    const std::string token(32,'a'),other(32,'b');const auto hash=account_hash(token);
    CHECK(hash.size()==64 && hash!=token && hash!=account_hash(other));
    CHECK(!credential_hex(hash,32));
    for(const std::string& invalid:{std::string(31,'a'),std::string(32,'A'),std::string(32,'g'),std::string(32,'a')+'\0'})
        CHECK(!credential_hex(invalid,32));
    const PublicProfile wide{UINT64_MAX,2147483647,0,10};
    auto parsed=parse_profile(profile_json(wide).dump());CHECK(parsed && parsed->player_id==UINT64_MAX && parsed->rp==2147483647);
    for(auto name:{"rp","xp","bp"}) {
        auto j=profile_json(wide);j[name]=2147483648ULL;CHECK(!parse_profile(j.dump()));
        j[name]=-1;CHECK(!parse_profile(j.dump()));j[name]=1.5;CHECK(!parse_profile(j.dump()));
        j[name]="0";CHECK(!parse_profile(j.dump()));
    }
    CHECK(!parse_profile("{\"player_id\":1,\"rp\":0,\"xp\":0,\"bp\":0,\"rp\":1}"));
    {
        SqliteResults db(argv[1]);db.seed_demo();SqliteDb raw(argv[1]);
        CHECK(db.register_account(101,hash)==SqliteResults::Registration::collision);
        CHECK(!db.profile_by_hash(hash));
        for(const auto* table:{"player_icons","wallets","careers","account_keys"}) {
            const auto sql=std::string("CREATE TRIGGER fail_guest BEFORE INSERT ON ")+table+
                " BEGIN SELECT RAISE(ABORT,'injected'); END;";
            raw.exec(sql.c_str());bool error=false;
            try{db.register_account(303,hash);}catch(const std::exception&){error=true;}CHECK(error);
            raw.exec("DROP TRIGGER fail_guest;");
            CHECK(schema_integer(raw,"SELECT count(*) FROM players")==2);
            CHECK(schema_integer(raw,"SELECT count(*) FROM account_keys")==0);
            CHECK(schema_integer(raw,"SELECT count(*) FROM wallets")==2);
        }
        CHECK(db.register_account(303,hash)==SqliteResults::Registration::created);
        CHECK(db.register_account(404,hash)==SqliteResults::Registration::collision);
        CHECK(db.register_account(303,account_hash(other))==SqliteResults::Registration::collision);
        const auto p=db.profile_by_hash(hash);CHECK(p && p->player_id==303 && p->bp==0 && p->rp==0 && p->xp==0);
        CHECK(!db.profile_by_hash(account_hash(other)));
        CHECK(db.icons(303)==std::vector<std::string>{"default"});
        raw.exec("UPDATE wallets SET bp=30 WHERE player_id='303'; UPDATE careers SET rp=16,xp=100 WHERE player_id='303';");
        CHECK(db.profile_by_hash(hash)->bp==30 && db.profile_by_hash(hash)->xp==100);
        raw.exec("DELETE FROM wallets WHERE player_id='303';");
        bool error=false;try{db.profile_by_hash(hash);}catch(const std::exception&){error=true;}CHECK(error);
        raw.exec("INSERT INTO wallets VALUES('303',30);");
        Statement q(raw.get(),"SELECT token_hash FROM account_keys");CHECK(q.next() && q.text_column(0)==hash);
    }
    {SqliteResults reopened(argv[1]);CHECK(reopened.profile_by_hash(hash)->player_id==303 && reopened.profile_by_hash(hash)->xp==100);}
    std::cout<<"ID/credential separation, entropy failure, hash-only persistence, atomic bootstrap, collision, profile range and restart: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
