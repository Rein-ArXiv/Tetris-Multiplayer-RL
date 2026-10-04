#include "client/account_controller.h"
#include "meta/account_bootstrap.h"
#include "client/account_panel.h"
#include "client/menu_model.h"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#define CHECK(x) do {if(!(x)){std::cerr<<"CHECK "<<__LINE__<<": "<<#x<<"\n";std::abort();}}while(0)
using namespace study_account_ui;
using namespace study_meta;
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered=false,release=false;
    void enter() {std::unique_lock<std::mutex> lock(mutex);entered=true;cv.notify_all();cv.wait(lock,[&]{return release;});}
    void wait_entered() {std::unique_lock<std::mutex> lock(mutex);CHECK(cv.wait_for(lock,std::chrono::seconds(3),[&]{return entered;}));}
    void open() {std::lock_guard<std::mutex> lock(mutex);release=true;cv.notify_all();}
};
struct State {Gate gate;int calls=0;std::atomic<bool> destroyed{false};bool throws=false;};
struct Worker {
    State& s;
    explicit Worker(State& value):s(value){}
    ~Worker(){s.destroyed=true;}
    AccountOutcome operator()(){++s.calls;s.gate.enter();if(s.throws)throw std::runtime_error("fixture");return {{BootstrapResult{BootstrapState::online,PublicProfile{7,0,0,0}}},false};}
};
template<class T> void finish(T& controller) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!controller.poll()){CHECK(std::chrono::steady_clock::now()<end);std::this_thread::yield();}
}
struct Store {
    bool writable=false;std::optional<IssuedAccount> account;int saves=0;
    bool available()const{return true;} std::string origin()const{return "fixture";}
    SavedAccount load()const{return account?SavedAccount{AccountLoad::ready,*account}:SavedAccount{AccountLoad::missing,{}};}
    bool save(const IssuedAccount& value){++saves;if(!writable)return false;account=value;return true;}
};
struct Api {
    int issues=0,verifies=0;bool ambiguous=false;
    bool valid()const{return true;} std::string origin()const{return "fixture";}
    std::optional<IssuedAccount> issue(){++issues;if(ambiguous)return {};return IssuedAccount{7,std::string(32,'a')};}
    VerifiedProfile verify(const std::string& token){CHECK(token==std::string(32,'a'));++verifies;return {VerifyState::accepted,PublicProfile{7,0,0,0}};}
};
struct SessionWorker {
    Store& store;Api& api;AccountBootstrap session;
    SessionWorker(Store& s,Api& a):store(s),api(a){}
    AccountOutcome operator()(){auto r=session.connect(store,api);return {r,session.has_unsaved_key()};}
};
int main(){
    // Result combinations never publish a profile before save + verification.
    for(int value=0;value<8;++value){
        BootstrapResult r{static_cast<BootstrapState>(value),PublicProfile{7,0,0,0}};
        auto v=present(r,true);CHECK(v.status==Status::unsaved&&v.has_unsaved_key&&!v.profile);
        v=present(r,false);CHECK(bool(v.profile)==(r.state==BootstrapState::online));
    }
    CHECK(present({BootstrapState::online,{}},false).status==Status::failed);
    CHECK(!may_connect({Status::creation_unconfirmed,false,{}}));
    // A blocked worker cannot block any poll or start a second job.
    State state;
    {
        Controller<Worker> controller(std::make_unique<Worker>(state));
        CHECK(controller.request());state.gate.wait_entered();
        int frames=0;
        for(;frames<10000;++frames){CHECK(!controller.poll());CHECK(!controller.request());CHECK(controller.busy());}
        CHECK(frames==10000&&state.calls==1);
        CHECK(controller.view().status==Status::busy&&!controller.view().profile);
        state.gate.open();finish(controller);
        CHECK(!controller.busy()&&!controller.poll());
        CHECK(controller.view().status==Status::online&&controller.view().profile->player_id==7);
        // Visibility is not controller lifetime: result remains when reopened.
        CHECK(controller.request());finish(controller);CHECK(state.calls==2);
    }
    CHECK(state.destroyed);
    State failure;failure.throws=true;failure.gate.open();
    {Controller<Worker> controller(std::make_unique<Worker>(failure));
     CHECK(controller.request());finish(controller);CHECK(controller.view().status==Status::failed&&!controller.busy());}
    // The second asynchronous call uses the SAME session and issued key.
    Store store;Api api;
    {Controller<SessionWorker> c(std::make_unique<SessionWorker>(store,api));
     CHECK(c.request());finish(c);CHECK(c.view().status==Status::unsaved&&c.view().has_unsaved_key);
     CHECK(api.issues==1&&api.verifies==0&&store.saves==1);
     store.writable=true;CHECK(c.request());finish(c);
     CHECK(c.view().status==Status::online&&!c.view().has_unsaved_key);
     CHECK(api.issues==1&&api.verifies==1&&store.saves==2);}
    Store absent;Api unknown;unknown.ambiguous=true;
    {Controller<SessionWorker> c(std::make_unique<SessionWorker>(absent,unknown));
     CHECK(c.request());finish(c);CHECK(c.view().status==Status::creation_unconfirmed);
     CHECK(!c.request()&&unknown.issues==1);}
    // Destruction waits for the borrowed worker before releasing its memory.
    State retiring;
    auto c=std::make_unique<Controller<Worker>>(std::make_unique<Worker>(retiring));
    retiring.destroyed=false;CHECK(c->request());retiring.gate.wait_entered();
    std::promise<void> begun;auto began=begun.get_future();
    std::thread closer([owned=std::move(c),&begun]()mutable{begun.set_value();owned.reset();});
    began.wait();CHECK(!retiring.destroyed);retiring.gate.open();closer.join();CHECK(retiring.destroyed);
    // Panel input: Escape/Back works during a job; confirm cannot fall through.
    study_ui::Input input{};
    CHECK(evaluate({Status::busy,false,{}},input,true,false,false)==Action::none);
    CHECK(evaluate({Status::busy,false,{}},input,true,true,false)==Action::back);
    CHECK(evaluate({},input,true,false,true)==Action::none);
    input.press=study_ui::Point{111,177};
    CHECK(evaluate({},input,true,false,false)==Action::back);
    input.press=study_ui::Point{111,133};
    CHECK(evaluate({},input,false,false,false)==Action::connect);
    CHECK(evaluate({Status::busy,false,{}},input,true,false,false)==Action::none);
    input.press=study_ui::Point{0,0};CHECK(evaluate({},input,true,false,false)==Action::none);
    study_menu::Preferences prefs;input.press=study_ui::Point{15,105};
    auto intent=study_menu::evaluate(prefs,study_menu::Focus::start,input,{});
    CHECK(intent.focus==study_menu::Focus::account&&intent.action==study_menu::Action::account);
    CHECK(!prefs.apply(intent.action));
    input={};study_menu::Keys keys;keys.down=true;keys.confirm=true;
    intent=study_menu::evaluate(prefs,study_menu::Focus::badge,input,keys);
    CHECK(intent.focus==study_menu::Focus::account&&intent.action==study_menu::Action::account);
    std::cout<<"Account UI: single-flight, ready-only polling, same-key retry, identity lifetime and input routing passed\n";
}
