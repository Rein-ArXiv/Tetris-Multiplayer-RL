#include "net/round_play.h"
#include <cstdio>

int main() {
    using namespace study_net;
    const auto round = study_round::Round::create_seeded(study_grid::Grid{}, 77);
    if (!round) return 1;
    const study_combat::Duel duel(*round, *round);
    RoundPlay host, peer;
    if (!host.prepare(1, duel, Side::host, 0) ||
        !peer.prepare(1, duel, Side::peer, 0)) return 1;
    Frame from_host, from_peer;
    if (host.capture(1, from_host).scope != RoundScope::inactive) return 1;
    std::puts("countdown: no local input history");
    if (!host.start() || host.capture(1, from_host).input != Put::stored) return 1;
    // The peer's local countdown ends later. Preserve current-round remote input.
    if (peer.receive(from_host).input != Put::stored ||
        peer.advance() != Advance::waiting || !peer.start()) return 1;
    if (peer.capture(0, from_peer).input != Put::stored ||
        host.receive(from_peer).input != Put::stored ||
        host.advance() != Advance::advanced || peer.advance() != Advance::advanced) return 1;
    std::puts("round 1: both simulations consumed tick 0");
    if (!peer.finish() || !peer.prepare(2, duel, Side::peer, 0)) return 1;
    if (peer.receive(from_host).scope != RoundScope::old_round) return 1;
    std::puts("round 2: delayed round-1 tick 0 rejected before history insertion");
    RoundBatch current{2, {}};
    current.inputs.count = 1;
    current.inputs.masks[0] = 2;
    Frame fresh;
    if (!encode_round_input(current, fresh) || peer.receive(fresh).input != Put::stored ||
        !peer.start() || peer.capture(0, from_peer).input != Put::stored ||
        peer.advance() != Advance::advanced) return 1;
    std::puts("round 2: fresh tick 0 consumed");
}
