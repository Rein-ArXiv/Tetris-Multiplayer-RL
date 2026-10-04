#include "meta/outbox_delivery.h"
#include <iostream>
#include <cstdlib>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::cerr << "line " << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
using namespace study_meta;
const std::string origin = "http://127.0.0.1:18083";
const study_net::MatchRecord request{121,1,101,202,12,40,20,1,0,study_net::MatchRecord::a};
const study_net::Receipt receipt{121,7,101,202};
OutboxDocument pending() { return {origin,request,OutboxStatus::pending,{}}; }
struct FakeStore {
    OutboxDocument document=pending(); int saves=0, fail_at=0; bool failing=false;
    LoadedOutbox load() const { return {OutboxLoad::ready, document}; }
    bool save(const OutboxDocument& next) {
        ++saves; if (failing || saves==fail_at) return false;
        document=next; return true;
    }
};
struct FakeApi {
    std::string address=::origin; int calls=0; std::vector<study_net::MatchRecord> received;
    study_net::Reply reply{study_net::Reply::confirmed,receipt};
    bool valid() const { return true; }
    const std::string& origin() const { return address; }
    study_net::Reply operator()(const study_net::MatchRecord& record, std::chrono::milliseconds) noexcept {
        ++calls; received.push_back(record); return reply;
    }
};
struct Clock {
    std::chrono::steady_clock::time_point time{};
    auto now() const { return time; }
    void wait(std::chrono::milliseconds ms) { time+=ms; }
};
template<class Store> DeliveryResult drive(OutboxDelivery<Store>& session, FakeApi& api) {
    Clock clock;
    return session.deliver(api,[&]{return clock.now();},[&](auto ms){clock.wait(ms);},std::chrono::milliseconds{5000});
}
void states() {
    FakeStore store;FakeApi api;OutboxDelivery session(store);
    store.failing=true;
    CHECK(drive(session,api).state==DeliveryState::storage_blocked && api.calls==0);
    store.failing=false;store.fail_at=3; // request flush succeeds; receipt save fails
    const auto confirmed=drive(session,api);
    CHECK(confirmed.state==DeliveryState::confirmed && !confirmed.local_saved && confirmed.receipt->row==7);
    CHECK(store.document.status==OutboxStatus::pending && api.calls==1);
    store.failing=true;
    CHECK(drive(session,api).state==DeliveryState::confirmed && api.calls==1);
    store.failing=false;
    CHECK(drive(session,api).local_saved && api.calls==1);
    OutboxDelivery reopened(store);FakeApi unused;
    CHECK(drive(reopened,unused).state==DeliveryState::confirmed && unused.calls==0);
    FakeStore uncertain;FakeApi lost;lost.reply={};OutboxDelivery first(uncertain);
    CHECK(drive(first,lost).state==DeliveryState::unconfirmed && lost.calls==3);
    for (const auto& record:lost.received) CHECK(record==request);
    CHECK(uncertain.document.status==OutboxStatus::pending);
    OutboxDelivery second(uncertain);FakeApi success;
    CHECK(drive(second,success).state==DeliveryState::confirmed && success.received.front()==request);
    FakeStore stopped;FakeApi bad;bad.reply={study_net::Reply::stopped,{}};OutboxDelivery stop(stopped);
    CHECK(drive(stop,bad).state==DeliveryState::stopped && bad.calls==1);
    OutboxDelivery stop_again(stopped);CHECK(drive(stop_again,bad).state==DeliveryState::stopped && bad.calls==1);
    FakeStore mismatch;FakeApi wrong;wrong.address="http://127.0.0.1:18084";OutboxDelivery scoped(mismatch);
    CHECK(drive(scoped,wrong).state==DeliveryState::storage_blocked && wrong.calls==0 && mismatch.saves==0);
    FakeStore malformed;FakeApi wrong_receipt;wrong_receipt.reply.receipt.key=122;OutboxDelivery inspect(malformed);
    CHECK(drive(inspect,wrong_receipt).state==DeliveryState::stopped && wrong_receipt.calls==1);
}
void wire() {
    auto doc=pending();auto encoded=outbox_json(doc);
    CHECK(parse_outbox(encoded)->request==request);
    for (const Json& value:{Json(0),Json(2),Json(true),Json("1"),Json(nullptr)}) {
        auto j=Json::parse(encoded);j["version"]=value;CHECK(!parse_outbox(j.dump()));
    }
    auto j=Json::parse(encoded);j["status"]="future";CHECK(!parse_outbox(j.dump()));
    j=Json::parse(encoded);j["receipt"]=receipt_json(receipt).dump();CHECK(!parse_outbox(j.dump()));
    j["status"]="confirmed";CHECK(parse_outbox(j.dump()));
    j["receipt"]="";CHECK(!parse_outbox(j.dump()));
    j=Json::parse(encoded);auto bad_request=j["request"].get<std::string>();bad_request[0]='[';j["request"]=bad_request;CHECK(!parse_outbox(j.dump()));
    CHECK(!parse_outbox(encoded+std::string(1,'\0')));
    doc.request.key=UINT64_MAX;doc.request.round=UINT64_MAX;doc.request.score_a=UINT64_MAX;
    CHECK(parse_outbox(outbox_json(doc))->request==doc.request);
    doc.status=OutboxStatus::confirmed;doc.receipt=receipt;CHECK(!valid_outbox(doc));
    doc=pending();doc.request.player_b=101;CHECK(!valid_outbox(doc));
    doc=pending();doc.status=static_cast<OutboxStatus>(88);CHECK(!valid_outbox(doc));
}
bool fail_before(const std::string&,const std::string&) { return false; }
bool fail_after(const std::string& path,const std::string& content) {
    CHECK(study_files::write_private_file(path,content));return false;
}
void files() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("study-outbox-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(fs::create_directory(root));
    struct Cleanup {fs::path path;~Cleanup(){std::error_code e;fs::remove_all(path,e);}} cleanup{root};
    {
        LocalOutbox store(root,origin);CHECK(store.available() && store.load().state==OutboxLoad::missing);
        LocalOutbox busy(root,origin);CHECK(!busy.available());
        CHECK(store.prepare(request));CHECK(!store.prepare(request));
        auto different=pending();++different.request.key;CHECK(!store.save(different));
        different=pending();++different.request.score_a;CHECK(!store.save(different));
        auto done=pending();done.status=OutboxStatus::confirmed;done.receipt=receipt;
        CHECK(store.save(done));CHECK(!store.save(pending()));
        ++done.receipt->row;CHECK(!store.save(done));
        CHECK(store.load().document->receipt->row==7);
    }
    {LocalOutbox other(root,"http://127.0.0.1:18084");CHECK(other.load().state==OutboxLoad::blocked);CHECK(!other.prepare(request));}
    {std::ofstream f(root/"outbox.json");f<<"damaged";}
    {LocalOutbox broken(root,origin);CHECK(broken.load().state==OutboxLoad::blocked);CHECK(!broken.prepare(request));}
    fs::remove(root/"outbox.json");
    {LocalOutbox before(root,origin,fail_before);CHECK(!before.prepare(request));CHECK(before.load().state==OutboxLoad::missing);}
    {LocalOutbox after(root,origin,fail_after);CHECK(!after.prepare(request));CHECK(after.load().state==OutboxLoad::ready);}
    {LocalOutbox visible(root,origin,fail_before);OutboxDelivery retry(visible);FakeApi api;
     CHECK(drive(retry,api).state==DeliveryState::storage_blocked && api.calls==0);}
    {LocalOutbox resumed(root,origin);OutboxDelivery retry(resumed);FakeApi api;
     CHECK(drive(retry,api).state==DeliveryState::confirmed && api.calls==1);}
}
int main() {wire();states();files();std::cout<<"Outbox: durable request barrier, same operation, receipt knowledge survives local failure, restart/stopped/origin/file guards passed\n";}
