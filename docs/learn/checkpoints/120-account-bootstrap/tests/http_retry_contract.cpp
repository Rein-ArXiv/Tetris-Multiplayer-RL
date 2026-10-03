#include "meta/http_sender.h"
#include "meta/retry_submission.h"
#include <atomic>
#include <thread>
#include <vector>
#include <limits>
#include <iostream>
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::cerr<<"failed "<<__LINE__<<": " #x "\n";std::exit(1);} } while(0)
using namespace study_meta;
using namespace study_net;
using Millis=std::chrono::milliseconds;
const MatchRecord record{119,1,101,202,60,10,0,1,0,MatchRecord::a};
Reply confirmed(const MatchRecord& r) noexcept {return {Reply::confirmed,{r.key,1,r.player_a,r.player_b}};}
struct FakeClock {
    Millis elapsed{0};
    std::vector<Millis> sleeps;
    auto now()const{return std::chrono::steady_clock::time_point{}+elapsed;}
    void wait(Millis delay){CHECK(delay.count()>=0);sleeps.push_back(delay);elapsed+=delay;}
};
template<class Sender>
RetryExit run(MatchSubmission& sub,Sender&& sender,FakeClock& clock,Millis budget=Millis{5000}) {
    return retry_submission(sub,sender,[&]{return clock.now();},[&](Millis d){clock.wait(d);},budget);
}
void schedule_contract() {
    CHECK(http_action(200)==HttpAction::inspect_receipt);
    for(int status:{429,500,503,599}) CHECK(http_action(status)==HttpAction::retry);
    for(int status:{0,100,199,201,204,301,400,401,403,409,428,430,499,600})
        CHECK(http_action(status)==HttpAction::stop);
    CHECK(retry_backoff(0)==Millis{0});CHECK(retry_backoff(1)==Millis{100});
    CHECK(retry_backoff(std::numeric_limits<unsigned>::max())==Millis{200});
    CHECK(retry_after_seconds("0")==Millis{0});CHECK(retry_after_seconds("001")==Millis{1000});
    for(const auto* s:{"","-1","+1","1.0","1e1"," 1","1 ","Wed, 21 Oct 2015 07:28:00 GMT","18446744073709551616"})
        CHECK(!retry_after_seconds(s));
    const auto bound=std::numeric_limits<Millis::rep>::max()/1000;
    CHECK(retry_after_seconds(std::to_string(bound))->count()==bound*1000);
    CHECK(!retry_after_seconds(std::to_string(static_cast<std::uint64_t>(bound)+1)));
    FakeClock clock;MatchSubmission sub(3);CHECK(sub.prepare(record));int calls=0;
    std::vector<Millis> limits;
    auto transient=[&](const MatchRecord& r,Millis limit)noexcept{
        CHECK(r==record);limits.push_back(limit);clock.elapsed+=Millis{20};
        return ++calls==3 ? confirmed(r) : Reply{};
    };
    CHECK(run(sub,transient,clock)==RetryExit::confirmed);
    CHECK(calls==3 && clock.sleeps==std::vector<Millis>({Millis{100},Millis{200}}));
    CHECK(limits==std::vector<Millis>({Millis{2000},Millis{2000},Millis{2000}}));
    CHECK(run(sub,transient,clock)==RetryExit::confirmed && calls==3);
    for(auto budget:{Millis{0},Millis{-1},Millis{std::numeric_limits<Millis::rep>::min()}}) {
        FakeClock t;MatchSubmission no_time(3);CHECK(no_time.prepare(record));
        CHECK(run(no_time,transient,t,budget)==RetryExit::budget_exhausted);
        CHECK(no_time.attempts()==0);
    }
    FakeClock t;MatchSubmission one(1);CHECK(one.prepare(record));
    CHECK(run(one,[](const auto&,Millis)noexcept{return Reply{};},t)==RetryExit::budget_exhausted);
    CHECK(one.attempts()==1 && t.sleeps.empty() && one.state()==MatchSubmission::SubmissionState::unconfirmed);
    FakeClock short_clock;MatchSubmission short_sub(3);CHECK(short_sub.prepare(record));
    CHECK(run(short_sub,[](const auto&,Millis)noexcept{return Reply{};},short_clock,Millis{100})==RetryExit::budget_exhausted);
    CHECK(short_sub.attempts()==1 && short_clock.sleeps.empty());
    FakeClock hinted;MatchSubmission hinted_sub(3);CHECK(hinted_sub.prepare(record));int n=0;
    CHECK(run(hinted_sub,[&](const auto& r,Millis)noexcept{
        return ++n==1 ? Reply{Reply::unconfirmed,{},Millis{1000}} : confirmed(r);
    },hinted)==RetryExit::confirmed);
    CHECK(hinted.sleeps==std::vector<Millis>{Millis{1000}});
    FakeClock too_late;MatchSubmission pending(3);CHECK(pending.prepare(record));
    CHECK(run(pending,[](const auto&,Millis)noexcept{
        return Reply{Reply::unconfirmed,{},Millis{5000}};
    },too_late)==RetryExit::budget_exhausted);
    CHECK(too_late.sleeps.empty() && pending.attempts()==1 && pending.request()->key==119);
    FakeClock overslept;MatchSubmission missed(3);CHECK(missed.prepare(record));
    CHECK(retry_submission(missed,[](const auto&,Millis)noexcept{return Reply{};},
        [&]{return overslept.now();},[&](Millis){overslept.elapsed+=Millis{1000};},Millis{500})==RetryExit::budget_exhausted);
    CHECK(missed.attempts()==1);
    FakeClock slow;MatchSubmission late_success(3);CHECK(late_success.prepare(record));
    CHECK(run(late_success,[&](const auto& r,Millis limit)noexcept{
        CHECK(limit==Millis{50});slow.elapsed+=Millis{80};return confirmed(r);
    },slow,Millis{50})==RetryExit::confirmed);
    FakeClock stopped_clock;MatchSubmission stopped(3);CHECK(stopped.prepare(record));
    CHECK(run(stopped,[](const auto&,Millis)noexcept{return Reply{Reply::stopped,{}};},stopped_clock)==RetryExit::stopped);
    CHECK(stopped.attempts()==1 && !stopped.receipt() && *stopped.request()==record);
    CHECK(run(stopped,transient,stopped_clock)==RetryExit::stopped && calls==3);
}
void http_contract() {
    httplib::Server server;std::atomic<int> mode{0},calls{0},writes{0};
    std::string first_body;std::atomic<bool> same{true};
    server.new_task_queue=[] {return new httplib::ThreadPool(2);};
    server.Post("/study/v1/matches",[&](const httplib::Request& req,httplib::Response& res){
        const int call=++calls;const int m=mode;
        if(call==1) first_body=req.body;else if(first_body!=req.body)same=false;
        if(m==0 && call==1) {++writes;res.status=503;res.set_content("commit happened; response unavailable","text/plain");return;}
        if(m>=400) {res.status=m;res.set_content("opaque failure body","text/plain");return;}
        if(m==1) {res.set_content("{}","application/json");return;}
        if(m==2) {auto wrong=confirmed(record).receipt;++wrong.key;res.set_content(receipt_json(wrong).dump(),"application/json");return;}
        if(m==3 && call==1) {res.status=429;res.set_header("Retry-After","1");return;}
        if(m==4) {res.status=503;res.set_header("Retry-After","10");return;}
        if(m==5) {res.status=503;res.set_header("Retry-After","Wed, 21 Oct 2015 07:28:00 GMT");return;}
        if(m==7) {res.status=429;res.set_header("Retry-After","1");res.set_header("Retry-After","2");return;}
        if(m==8) {res.status=503;res.set_header("Retry-After","18446744073709551616");return;}
        if(m==6 && call==1) {std::this_thread::sleep_for(Millis{120});}
        res.set_content(receipt_json(confirmed(record).receipt).dump(),"application/json");
    });
    const int port=server.bind_to_any_port("127.0.0.1");CHECK(port>0);
    std::thread worker([&]{server.listen_after_bind();});
    struct Stop {httplib::Server& s;std::thread& t;~Stop(){s.stop();t.join();}} stop{server,worker};
    server.wait_until_ready();
    for(int m:{0,1,2,3,4,5,7,8,400,401,403,409,429,500,503}) {
        mode=m;calls=0;same=true;first_body.clear();
        MatchSubmission sub(3);CHECK(sub.prepare(record));FakeClock clock;
        const auto result=run(sub,HttpSender(port),clock);
        if(m==0 || m==3) {
            CHECK(result==RetryExit::confirmed && calls==2);
            CHECK(clock.sleeps[0]==Millis{m==3 ? 1000 : 100});
        } else if(m==4 || m==429 || m==500 || m==503) {
            CHECK(result==RetryExit::budget_exhausted);
            CHECK(calls==(m==4 ? 1 : 3));
        } else {CHECK(result==RetryExit::stopped && calls==1);}
        CHECK(same && record_json(record).dump()==first_body);
    }
    CHECK(writes==1);
    mode=6;calls=0;first_body.clear();
    const auto timed=HttpSender(port)(record,Millis{30});
    CHECK(timed.kind==Reply::unconfirmed); // read timeout does not prove rollback
    std::this_thread::sleep_for(Millis{150});
    CHECK(HttpSender(port)(record,Millis{1000}).kind==Reply::confirmed);
    CHECK(HttpSender(0)(record).kind==Reply::stopped);
}
int main(){schedule_contract();http_contract();std::cout<<"HTTP retry: classification, stable body/key, waits, phase budgets, stop/unknown outcomes and timeout passed\n";}
