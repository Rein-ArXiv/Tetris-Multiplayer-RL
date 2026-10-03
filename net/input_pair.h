#pragma once
#include <cstdint>
#include <unordered_map>
namespace net {
// Called by the simulation owner. A stored zero is a neutral input; absence is
// not. Read both candidates before publishing either output. RemoteLookup must
// be a non-consuming lookup, and the two output references must be distinct.
template<class RemoteLookup>
bool read_input_pair(const std::unordered_map<uint32_t,uint8_t>& local,
                     uint32_t tick, RemoteLookup remote,
                     uint8_t& local_out, uint8_t& remote_out) {
    const auto found = local.find(tick);
    if (found == local.end()) return false;
    const uint8_t candidate_local = found->second;
    uint8_t candidate_remote = 0;
    if (!remote(tick, candidate_remote)) return false;
    local_out = candidate_local;
    remote_out = candidate_remote;
    return true;
}
} // namespace net
