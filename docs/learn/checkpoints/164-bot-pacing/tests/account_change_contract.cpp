#include "meta/sqlite_results.h"
#include "meta/account_crypto.h"
#include "meta/admission_tickets.h"
#include <filesystem>
#include <future>
#include <iostream>
using namespace study_meta;
static void require(bool v) {if(!v)throw std::runtime_error("change contract");}
static std::string key(char c) {return std::string(study_credentials::account_bytes*study_credentials::hex_per_byte,c);}
static std::string recovery(char c) {return std::string(recovery_prefix)+std::string(recovery_bytes*study_credentials::hex_per_byte,c);}
int main(int argc,char** argv) {
    try {
        require(argc==2 && !std::filesystem::exists(argv[1]));
        const std::string path=argv[1];SqliteResults store(path);store.seed_shop();
        require(store.register_account(7,account_hash(key('a')))==SqliteResults::Registration::created);
        require(store.register_account(9,account_hash(key('b')))==SqliteResults::Registration::created);
        {SqliteDb raw(path);raw.exec("UPDATE wallets SET bp=321 WHERE player_id='7';UPDATE careers SET rp=12,xp=45 WHERE player_id='7'");}
        AdmissionTickets<2> tickets;Ticket ticket{};const auto now=AdmissionTickets<2>::Clock::now();
        require(tickets.issue(ticket,{7,0},now,std::chrono::seconds(1))==IssueTicket::issued);
        require(store.credential_is_current(7,0));
        const ChangeRequest backup{ChangeKind::backup,key('a'),key('a'),recovery('c')};
        auto r=store.change_account(backup);require(r.status==ChangeStatus::ok && r.player_id==7 && r.epoch==1);
        auto admission=tickets.consume(ticket,now);require(admission && !store.credential_is_current(admission->player,admission->epoch));
        require(store.change_account(backup).epoch==1);
        const auto before=*store.profile_by_hash(account_hash(key('a')));
        require(before.bp==321 && before.rp==12 && before.xp==45);
        require(store.change_account({ChangeKind::rotate,key('a'),key('b'),recovery('d')}).status==ChangeStatus::conflict);
        require(store.change_account({ChangeKind::backup,key('b'),key('b'),recovery('e')}).status==ChangeStatus::ok);
        require(store.change_account({ChangeKind::rotate,key('a'),key('d'),recovery('e')}).status==ChangeStatus::conflict);
        const ChangeRequest rotate{ChangeKind::rotate,key('a'),key('d'),recovery('f')};
        for(auto stop:{ChangePoint::after_update,ChangePoint::before_commit}) {
            r=store.change_account(rotate,[&](ChangePoint p){if(p==stop)throw std::runtime_error("injected");});
            require(r.status==ChangeStatus::storage_error && r.player_id==0);
            require(store.profile_by_hash(account_hash(key('a'))).has_value() && !store.profile_by_hash(account_hash(key('d'))));
            require(store.credential_is_current(7,1));
        }
        {SqliteDb raw(path);raw.exec("CREATE TRIGGER reject_change BEFORE UPDATE ON account_keys BEGIN SELECT RAISE(ABORT,'injected');END;");}
        require(store.change_account(rotate).status==ChangeStatus::storage_error);
        {SqliteDb raw(path);raw.exec("DROP TRIGGER reject_change");}
        r=store.change_account(rotate);require(r.status==ChangeStatus::ok && r.epoch==2);
        require(!store.profile_by_hash(account_hash(key('a'))));
        require(store.change_account(rotate).epoch==2);
        require(store.change_account({ChangeKind::backup,key('d'),key('d'),recovery('f')}).status==ChangeStatus::invalid_request);
        require(store.change_account({ChangeKind::rotate,key('d'),key('1'),recovery('f')}).status==ChangeStatus::invalid_request);
        require(store.change_account({ChangeKind::recover,recovery('f'),key('d'),recovery('2')}).status==ChangeStatus::invalid_request);
        require(store.credential_is_current(7,2));
        require(store.change_account({ChangeKind::rotate,key('a'),key('1'),recovery('2')}).status==ChangeStatus::invalid_credential);
        require(store.change_account({ChangeKind::recover,recovery('c'),key('1'),recovery('2')}).status==ChangeStatus::invalid_credential);
        SqliteResults other_connection(path);
        const ChangeRequest recover_a{ChangeKind::recover,recovery('f'),key('1'),recovery('2')};
        const ChangeRequest recover_b{ChangeKind::recover,recovery('f'),key('3'),recovery('4')};
        auto a=std::async(std::launch::async,[&]{return store.change_account(recover_a);});
        auto b=std::async(std::launch::async,[&]{return other_connection.change_account(recover_b);});
        const auto ar=a.get(),br=b.get();
        require((ar.status==ChangeStatus::ok && br.status==ChangeStatus::invalid_credential) ||
                (br.status==ChangeStatus::ok && ar.status==ChangeStatus::invalid_credential));
        const auto winner=ar.status==ChangeStatus::ok?recover_a:recover_b;
        require(store.change_account(winner).epoch==3);
        require(store.change_account(rotate).status==ChangeStatus::invalid_credential);
        const auto profile=*store.profile_by_hash(account_hash(winner.next_token));
        require(profile.player_id==7 && profile.bp==before.bp && profile.rp==before.rp && profile.xp==before.xp);
        require(store.inventory(account_hash(winner.next_token)).status==ShopStatus::ok);
        {SqliteDb raw(path);raw.exec("UPDATE account_keys SET auth_epoch=9223372036854775807 WHERE player_id='7'");}
        r=store.change_account({ChangeKind::rotate,winner.next_token,key('5'),recovery('6')});
        require(r.status==ChangeStatus::storage_error && store.profile_by_hash(account_hash(winner.next_token)).has_value());
        require(!store.credential_is_current(7,std::numeric_limits<std::uint64_t>::max()));
        for(auto invalid:{ChangeRequest{ChangeKind::backup,winner.next_token,key('9'),recovery('8')},
                         ChangeRequest{ChangeKind::rotate,winner.next_token,winner.next_token,recovery('8')},
                         ChangeRequest{ChangeKind::recover,recovery('8'),key('9'),recovery('8')}})
            require(store.change_account(invalid).status==ChangeStatus::invalid_request);
        std::cout<<"atomic replacement, collision/exception rollback, last receipt, independent connections, epoch and profile preservation passed\n";
        return 0;
    }catch(const std::exception&){std::cerr<<"account change contract failed\n";return 1;}
}
