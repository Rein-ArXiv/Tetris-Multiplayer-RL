#include "net/admission_session.h"
#include "net/monotonic_id.h"
#include "net/probe_frame.h"
#include <vector>
#include <stdexcept>
#include <cstdio>
using namespace study_net;
void check(bool ok) { if (!ok) throw std::runtime_error("state contract"); }
std::vector<std::uint8_t> wire() {
    Frame admission{50, {}, 2}; admission.payload[0] = 1;
    Frame ready{17, {}, 1}; ready.payload[0] = 1;
    std::vector<std::uint8_t> result;
    for (const auto& f : {admission, ready, probe_frame(3, 9)}) {
        EncodedFrame e; check(encode_frame(f,e));
        result.insert(result.end(),e.bytes.begin(),e.bytes.begin()+e.size);
    }
    return result;
}
void drive(AdmissionSession& s, int& games) {
    Frame f;
    for (;;) {
        const auto effect=s.step(f);
        if(effect==AdmissionSession::Effect::none) return;
        if(effect==AdmissionSession::Effect::authenticate) {
            check(s.stage()==AdmissionSession::Stage::auth && !s.can_read());
            check(!s.resume_auth(s.id()+1,true));
            check(s.resume_auth(s.id(),true));
            check(!s.resume_auth(s.id(),true)); // Duplicate response is not a new admission.
        } else if(effect==AdmissionSession::Effect::ready) {
            check(!s.can_read() && s.ready());
            check(s.begin_forward()); check(!s.begin_forward());
        } else if(effect==AdmissionSession::Effect::game) {
            check(matches_probe(f,3,9)); ++games;
        } else check(false);
    }
}
int main() {
    const auto bytes=wire();
    for(std::size_t cut=0;cut<=bytes.size();++cut) {
        AdmissionSession s(7);int games=0;
        check(s.append(bytes.data(),cut));drive(s,games);
        check(s.append(bytes.data()+cut,bytes.size()-cut));drive(s,games);
        check(games==1 && s.pending_bytes()==0);
    }
    AdmissionSession s(8);Frame f;check(s.append(bytes.data(),bytes.size()));
    check(s.step(f)==AdmissionSession::Effect::authenticate);
    const auto tail=s.pending_bytes();check(tail>0 && s.step(f)==AdmissionSession::Effect::none);
    check(tail==s.pending_bytes());s.close();s.close();
    check(!s.resume_auth(8,true) && !s.begin_forward() && !s.ready());
    AdmissionSession denied(9);check(denied.append(bytes.data(),bytes.size()));
    check(denied.step(f)==AdmissionSession::Effect::authenticate);check(denied.resume_auth(9,false));
    check(denied.step(f)==AdmissionSession::Effect::closed);
    AdmissionSession malformed(10);const std::uint8_t bad[]={0,0};
    check(malformed.append(bad,2));check(malformed.step(f)==AdmissionSession::Effect::closed);
    AdmissionSession wrong(11);Frame other=probe_frame(3,9);EncodedFrame encoded;
    check(encode_frame(other,encoded));check(wrong.append(encoded.bytes.data(),encoded.size));
    check(wrong.step(f)==AdmissionSession::Effect::closed);
    // READY in forward is a protocol error; duplicate acceptance cannot replay a transition.
    AdmissionSession duplicate(12);int games=0;check(duplicate.append(bytes.data(),9));drive(duplicate,games);
    check(duplicate.append(bytes.data()+5,4));check(duplicate.step(f)==AdmissionSession::Effect::closed);
    MonotonicId ids(UINT32_MAX-1);check(ids.take()==UINT32_MAX-1);check(ids.take()==UINT32_MAX);
    check(ids.take()==0 && ids.take()==0);MonotonicId zero(0);check(zero.take()==0);
    std::puts("state contracts: all split points, buffered transitions, stale/duplicate auth, rejection, ID exhaustion");
}
