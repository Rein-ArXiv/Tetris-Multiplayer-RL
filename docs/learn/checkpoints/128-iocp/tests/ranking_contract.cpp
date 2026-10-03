#include "meta/sqlite_results.h"
#include "client/ranking_controller.h"
#include "client/ranking_panel.h"
#include "client/ranking_text.h"
#include "client/labels.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <cstdlib>
#define CHECK(x) do{if(!(x)){std::cerr<<"CHECK "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
using namespace study_meta;
struct Worker {
    std::optional<Ranking> value;bool throws=false;
    std::optional<Ranking> operator()() {if(throws)throw std::runtime_error("fixture");return value;}
};
template<class T>void finish(T& c){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!c.poll()){CHECK(std::chrono::steady_clock::now()<end);std::this_thread::yield();}}
int main(){
    CHECK(parse_ranking("[]")&&parse_ranking("[]")->empty());
    const Ranking valid{{9,100},{10,100},{UINT64_MAX,0}};
    auto got=parse_ranking(ranking_json(valid).dump());CHECK(got&&got->size()==3&&got->back().player_id==UINT64_MAX);
    for(const auto* body:{"{}","null","[1]","[[1]]","[{\"player_id\":1,\"rp\":{}}]",
        "[{\"player_id\":1,\"rp\":0,\"rp\":5}]","[{\"player_id\":1,\"rp\":0,\"x\":0}]",
        "[{\"player_id\":0,\"rp\":0}]","[{\"player_id\":true,\"rp\":0}]",
        "[{\"player_id\":18446744073709551616,\"rp\":0}]","[{\"player_id\":1,\"rp\":2147483648}]",
        "[{\"player_id\":1,\"rp\":-1}]","[{\"player_id\":1,\"rp\":1.0}]",
        "[{\"player_id\":\"1\",\"rp\":0}]","[{\"player_id\":1,\"rp\":0}] false",
        "[{\"player_id\":10,\"rp\":0},{\"player_id\":9,\"rp\":0}]",
        "[{\"player_id\":1,\"rp\":1},{\"player_id\":1,\"rp\":0}]",
        "[{\"player_id\":1,\"rp\":0},{\"player_id\":2,\"rp\":1}]",
        "[{\"player_id\":1,\"rp\":0},{\"player_id\":2,\"rp\":0},{\"player_id\":3,\"rp\":0},{\"player_id\":4,\"rp\":0}]"})CHECK(!parse_ranking(body));
    CHECK(!parse_ranking(std::string(1025,' ')));CHECK(!parse_ranking(std::string("[]\0",3)));
    CHECK(parse_ranking("[{\"player_id\":1,\"rp\":2147483647}]")->at(0).rp==2147483647);
    CHECK(!valid_ranking({{1,0},{1,0}})&&!valid_ranking({{1,-1}}));
    // Canonical TEXT identifiers sort numerically even beyond signed SQLite INTEGER.
    SqliteResults store(":memory:");CHECK(store.ranking().empty());
    for(auto id:{std::uint64_t(100),std::uint64_t(10),std::uint64_t(9),std::uint64_t(2),UINT64_MAX})store.add_player(id,"Fixture");
    auto rows=store.ranking();CHECK(rows.size()==3&&rows[0].player_id==2&&rows[1].player_id==9&&rows[2].player_id==10);
    study_net::MatchRecord record{1,1,9,10,60,10,0,1,0,study_net::MatchRecord::MatchWinner::a};
    CHECK(store.put(record).status==SqlStatus::accepted);rows=store.ranking();
    CHECK(rows[0].player_id==9&&rows[0].rp>0&&rows[1].player_id==2&&rows[2].player_id==10);
    CHECK(store.put(record).status==SqlStatus::accepted);
    CHECK(ranking_json(rows)==ranking_json(store.ranking())); // replay changes no rank value
    SqliteResults large(":memory:");
    for(auto id:{UINT64_MAX,std::uint64_t(INT64_MAX)+1,std::uint64_t(9)})large.add_player(id,"Large");
    rows=large.ranking();CHECK(rows[0].player_id==9&&rows[1].player_id==std::uint64_t(INT64_MAX)+1&&rows[2].player_id==UINT64_MAX);
    // Full uint64 labels fit separate bounded paragraphs without dropping digits.
    for(auto id:{std::uint64_t(1),std::uint64_t(9999999999),std::uint64_t(10000000000),UINT64_MAX}){
        auto text=study_ranking_ui::row_text(2,{id,2147483647});
        CHECK(text[1].substr(3)+text[2]==std::to_string(id));
        for(auto& part:text)CHECK(study_labels::decode(part));
    }
    // Result algebra: ready empty, ready data, failure and exception are distinct.
    using namespace study_ranking_ui;
    for(auto data:{std::optional<Ranking>{},std::optional<Ranking>{Ranking{}},std::optional<Ranking>{valid}}){
        Controller<Worker> c(std::make_unique<Worker>(Worker{data,false}));
        CHECK(c.view().status==Status::idle&&c.request()&&!c.request());finish(c);
        CHECK(c.view().status==(data?Status::ready:Status::failed));CHECK(!c.poll());
        CHECK(c.request());CHECK(c.view().status==Status::busy&&c.view().rows.empty());finish(c);
    }
    Controller<Worker> failing(std::make_unique<Worker>(Worker{{},true}));CHECK(failing.request());finish(failing);CHECK(failing.view().status==Status::failed);
    study_ui::Input input{};View busy{Status::busy,{}};
    CHECK(evaluate(busy,input,true,false,false)==Action::none);
    CHECK(evaluate(busy,input,true,true,false)==Action::back);
    CHECK(evaluate({},input,true,false,true)==Action::none);
    input.press=study_ui::Point{204,197};CHECK(evaluate({},input,true,false,false)==Action::back);
    std::cout<<"Ranking: numeric order/full IDs, saved results, replay stability, bounded wire, empty/error and job/UI contracts passed\n";
}
