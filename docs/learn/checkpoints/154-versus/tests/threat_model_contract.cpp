#include "net/bound_input_stream.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>
using namespace study_net;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
Frame request(std::uint64_t round = 9, std::uint32_t first = 0,
              std::initializer_list<std::uint8_t> masks = {0}) {
    check(masks.size() <= kMaxBatchInputs, "test input capacity");
    RoundBatch batch; batch.round = round; batch.inputs.first_tick = first;
    batch.inputs.count = masks.size(); std::copy(masks.begin(), masks.end(), batch.inputs.masks.begin());
    Frame out; check(encode_round_input(batch, out), "encode request"); return out;
}
int main() {
    try {
        for (auto args : {std::array<std::uint64_t,3>{0,11,22}, {9,0,22}, {9,11,0}, {9,11,11}}) {
            bool rejected = false;
            try { InputAuthority bad(args[0], args[1], args[2]); } catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "invalid trusted configuration");
        }
        InputAuthority gate(9,11,22); const auto valid = request();
        check(gate.submit(11,valid) == InputDecision::inactive, "inactive first");
        check(gate.start() && !gate.start(), "one start");
        check(gate.submit(0,valid) == InputDecision::unauthenticated, "missing identity");
        check(gate.submit(99,valid) == InputDecision::not_participant, "authenticated outsider");
        // All 255 unsupported types, including a correctly encoded result claim.
        for (unsigned type = 0; type <= 255; ++type) if (type != kRoundInputType) {
            auto forged = valid; forged.type = static_cast<std::uint8_t>(type);
            check(gate.submit(11,forged) == InputDecision::forbidden_type, "type allowlist");
        }
        auto bad = valid; bad.size = 33;
        check(gate.submit(11,bad) == InputDecision::malformed, "declared payload exceeds storage");
        bad = valid; --bad.size;
        check(gate.submit(11,bad) == InputDecision::malformed, "truncated payload");
        bad = valid; bad.payload[14] = 0x80;
        check(gate.submit(11,bad) == InputDecision::malformed, "unknown input bit");
        check(gate.submit(11,request(8)) == InputDecision::wrong_round, "old round");
        check(gate.submit(11,request(10)) == InputDecision::wrong_round, "future round");
        check(gate.submit(11,request(9,32)) == InputDecision::too_far, "window bound");
        check(gate.submit(11,request(9,UINT32_MAX)) == InputDecision::too_far, "wire tick cannot skip window");
        check(gate.next_tick() == 0, "denials did not advance tick");
        std::uint8_t a=77,b=88;
        check(!gate.take(a,b) && a==77 && b==88, "no claimed result creates input");
        check(gate.submit(11,request(9,1,{1})) == InputDecision::stored, "preexisting second tick");
        check(gate.submit(11,request(9,0,{2,2})) == InputDecision::conflict, "later conflict rejects whole batch");
        check(gate.submit(22,request(9,0,{0,0})) == InputDecision::stored, "other side independent");
        check(!gate.take(a,b), "first element rolled back on later conflict");
        check(gate.submit(11,request(9,0,{4,1})) == InputDecision::stored, "normal follows denial");
        check(gate.submit(11,request(9,0,{4,1})) == InputDecision::duplicate, "identical retry");
        check(!gate.take(a,a) && a==77, "aliased output is rejected");
        check(gate.take(a,b) && a==4 && b==0 && gate.next_tick()==1, "trusted host binding");
        check(gate.take(a,b) && a==1 && b==0 && gate.next_tick()==2, "both ticks exactly once");
        check(gate.submit(11,valid)==InputDecision::stale, "consumed tick stays stale");
        gate.close(); gate.close();
        check(!gate.start() && !gate.take(a,b) && gate.submit(11,valid)==InputDecision::inactive, "irreversible close");

        Frame forged = valid; forged.type = 42;
        EncodedFrame f,v; check(encode_frame(forged,f)&&encode_frame(valid,v), "wire encode");
        std::vector<std::uint8_t> bytes(f.bytes.begin(),f.bytes.begin()+f.size);
        bytes.insert(bytes.end(),v.bytes.begin(),v.bytes.begin()+v.size);
        // Every split of forged+normal input, then <=16-byte socket-read pieces.
        for (std::size_t split=0; split<=bytes.size(); ++split) {
            InputAuthority authority(9,11,22); check(authority.start(),"start split");
            BoundInputStream input(authority,11); std::size_t stored=0,denied=0;
            for (auto interval : {std::pair<std::size_t,std::size_t>{0,split},{split,bytes.size()}}) {
                auto pos=interval.first;
                while(pos<interval.second) {
                    const auto n=(std::min)(std::size_t{16},interval.second-pos);
                    BoundInputStream::Report report; check(input.feed(bytes.data()+pos,n,report),"stream shape");
                    stored+=report.stored;denied+=report.denied;pos+=n;
                }
            }
            check(stored==1&&denied==1&&authority.next_tick()==0,"semantic denial consumes one frame");
            check(authority.submit(22,valid)==InputDecision::stored&&authority.take(a,b)&&a==0&&b==0,"neutral input is present");
        }
        InputAuthority closed_boundary(9,11,22); check(closed_boundary.start(),"start boundary");
        BoundInputStream input(closed_boundary,11);BoundInputStream::Report report;
        const std::uint8_t invalid_length[]{0,0};
        check(!input.feed(invalid_length,2,report)&&input.failed(),"framing failure latches");
        check(!input.feed(v.bytes.data(),16,report),"no resync after corrupt boundary");
        std::puts("identity/state/type/shape/round/window boundaries; transactional conflicts; retries; all stream splits passed");
    } catch(const std::exception& e) {std::fprintf(stderr,"%s\n",e.what());return 1;}
}
