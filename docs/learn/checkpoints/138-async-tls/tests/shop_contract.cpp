#include "meta/sqlite_results.h"
#include "meta/account_crypto.h"
#include <filesystem>
#include <iostream>
#include <thread>
using namespace study_meta;
#define CHECK(x) do{if(!(x))throw std::runtime_error("check failed: " #x);}while(false)
int main(int argc,char** argv) {
 try {
    CHECK(argc==2);CHECK(!std::filesystem::exists(argv[1]));
    const std::string token(32,'a'),other(32,'b');const auto hash=account_hash(token),hash_b=account_hash(other);
    for(const auto* bad:{"{}","[]","null","{\"icon_id\":1}","{\"icon_id\":true}","{\"icon_id\":null}",
        "{\"icon_id\":\"ruby\",\"price\":0}","{\"icon_id\":\"ruby\",\"player_id\":1}",
        "{\"icon_id\":\"ruby\",\"icon_id\":\"gold\"}","{\"icon_id\":\"ruby\\u0000x\"}","{\"icon_id\":\"Ruby\"}"})
        CHECK(!parse_icon_choice(bad));
    CHECK(parse_icon_choice("{\"icon_id\":\"ruby\"}")=="ruby");
    CHECK(parse_icon_choice("{\"icon_id\":\"unknown\"}")=="unknown" && !shop_icon("unknown"));
    CHECK(!shop_icon(std::string("ruby\0x",6)));
    SqliteResults db(argv[1]);db.seed_shop();
    CHECK(db.register_account(101,hash)==SqliteResults::Registration::created);
    CHECK(db.register_account(202,hash_b)==SqliteResults::Registration::created);
    CHECK(db.inventory(hash).status==ShopStatus::ok);
    CHECK(db.choose_icon(hash,"ruby").status==ShopStatus::not_owned);
    CHECK(db.buy_icon(hash,"ruby").status==ShopStatus::insufficient_bp);
    CHECK(db.buy_icon(hash,"default").status==ShopStatus::already_owned);
    CHECK(db.buy_icon(hash,"missing").status==ShopStatus::unknown_icon);
    CHECK(db.buy_icon(account_hash(std::string(32,'c')),"ruby").status==ShopStatus::unknown_account);
    SqliteDb raw(argv[1]);raw.exec("UPDATE wallets SET bp=99 WHERE player_id='101';");
    CHECK(db.buy_icon(hash,"ruby").status==ShopStatus::insufficient_bp);
    raw.exec("UPDATE wallets SET bp=300 WHERE player_id='101';");
    for(const auto* effect:{"RAISE(ABORT,'injected')","RAISE(IGNORE)"}) {
        raw.exec((std::string("CREATE TRIGGER reject_grant BEFORE INSERT ON player_icons WHEN NEW.icon_id='ruby' BEGIN SELECT ")+effect+"; END;").c_str());
        CHECK(db.buy_icon(hash,"ruby").status==ShopStatus::storage_error);
        CHECK(db.balance(101)==300 && db.icons(101)==std::vector<std::string>{"default"});
        raw.exec("DROP TRIGGER reject_grant;");
    }
    SqliteResults second(argv[1]);ShopResult a{ShopStatus::storage_error,{}},b{ShopStatus::storage_error,{}};
    std::thread t1([&]{a=db.buy_icon(hash,"ruby");}),t2([&]{b=second.buy_icon(hash,"ruby");});t1.join();t2.join();
    CHECK((a.status==ShopStatus::ok && b.status==ShopStatus::already_owned) || (b.status==ShopStatus::ok && a.status==ShopStatus::already_owned));
    CHECK(db.balance(101)==200);
    CHECK(db.inventory(hash).view->selected=="default");
    CHECK(db.inventory(hash_b).view->owned==std::vector<std::string>{"default"});
    CHECK(db.choose_icon(hash_b,"ruby").status==ShopStatus::not_owned);
    CHECK(db.choose_icon(hash,"ruby").view->selected=="ruby");
    CHECK(db.choose_icon(hash,"ruby").status==ShopStatus::ok && db.balance(101)==200);
    raw.exec("UPDATE wallets SET bp=250 WHERE player_id='101';");
    CHECK(db.buy_icon(hash,"gold").view->profile.bp==0);
    CHECK(db.inventory(hash).view->selected=="ruby");
    raw.exec("DELETE FROM account_keys WHERE player_id='101';");
    CHECK(db.choose_icon(hash,"gold").status==ShopStatus::unknown_account);
    raw.exec(("INSERT INTO account_keys VALUES('101','"+hash+"');").c_str());
    {SqliteResults reopen(argv[1]);CHECK(reopen.inventory(hash).view->owned.size()==3 && reopen.inventory(hash).view->selected=="ruby");}
    std::cout<<"Catalog authority, choice parser, exact price, grant faults, concurrent debit once, ownership isolation, selection/retry, revocation and reopen: passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
