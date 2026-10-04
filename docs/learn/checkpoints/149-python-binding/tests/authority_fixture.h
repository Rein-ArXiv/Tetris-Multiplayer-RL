#pragma once
#include "net/authoritative_match.h"
#include "simulation/state_hash.h"
#include <algorithm>
#include <vector>
#include <stdexcept>
namespace study_authority_test {
inline constexpr std::uint64_t round_id=9,host_id=11,peer_id=22;
inline constexpr std::uint64_t test_budget=10000;
inline void require(bool ok) { if(!ok)throw std::runtime_error("authoritative simulation contract"); }
inline study_net::Frame frame(std::uint32_t first,const std::vector<std::uint8_t>& masks) {
    study_net::RoundBatch batch;batch.round=round_id;batch.inputs.first_tick=first;
    require(!masks.empty() && masks.size()<=study_net::kMaxBatchInputs);
    batch.inputs.count=masks.size();std::copy(masks.begin(),masks.end(),batch.inputs.masks.begin());
    study_net::Frame result;require(study_net::encode_round_input(batch,result));return result;
}
inline bool same(const study_round::Round& a,const study_round::Round& b) {
    const auto x=study_hash::state_bytes(a),y=study_hash::state_bytes(b);
    return x.ok() && y.ok() && x.size()==y.size() && std::equal(x.data(),x.data()+x.size(),y.data());
}
inline void same_duel(const study_combat::Duel& a,const study_combat::Duel& b) {
    require(same(a.left(),b.left()) && same(a.right(),b.right()));
}
}
