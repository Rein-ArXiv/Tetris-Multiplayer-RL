#pragma once
#include "core/canonical_bytes.h"
#include "simulation/round.h"

namespace study_hash {
// LRND schema 1: persistent Round state under this checkpoint's fixed rules.
// Presentation, derived ghost, last-report fields and FrameRunner are excluded.
using RoundBytes = Bytes<512>;

template<std::size_t N>
void append_queue(Bytes<N>& out, const study_next::Queue& queue) noexcept {
    out.u8(static_cast<std::uint8_t>(queue.size()));
    for (std::size_t i = 0; i < queue.size(); ++i)
        out.u8(static_cast<std::uint8_t>(*queue.peek(i)));
}

template<std::size_t N>
void append_source(Bytes<N>& out, const study_next::PieceSource& source) noexcept {
    if (const auto* scripted = source.scripted()) {
        out.u8(0); // Mode tag precedes the mode-specific payload.
        out.u8(static_cast<std::uint8_t>(scripted->size()));
        for (std::size_t i = 0; i < scripted->size(); ++i)
            out.u8(static_cast<std::uint8_t>(*scripted->at(i)));
        out.u8(static_cast<std::uint8_t>(scripted->cursor()));
    } else {
        const auto& seeded = *source.seeded();
        out.u8(1);
        out.u64(seeded.rng_state());
        out.u8(static_cast<std::uint8_t>(seeded.bag().remaining()));
        for (std::size_t i = 0; i < seeded.bag().remaining(); ++i)
            out.u8(static_cast<std::uint8_t>(*seeded.bag().at(i)));
    }
}

inline RoundBytes state_bytes(const study_round::Round& round) noexcept {
    RoundBytes out;
    for (auto byte : {'L','R','N','D'}) out.u8(static_cast<std::uint8_t>(byte));
    out.u32(1); // Change when encoding OR fixed rule meaning changes.
    out.u32(study_grid::Grid::kRows);
    out.u32(study_grid::Grid::kColumns);
    for (auto cell : round.board().cells()) out.u8(static_cast<std::uint8_t>(cell));
    out.u8(static_cast<std::uint8_t>(round.end_reason()));
    out.u8(static_cast<std::uint8_t>(round.kind()));
    out.i32(round.quarter());
    out.u8(round.active() ? 1 : 0);
    if (round.active()) {
        const auto& piece = *round.active();
        out.i32(piece.origin.row);
        out.i32(piece.origin.column);
        for (const auto& cell : piece.local) {
            out.i32(cell.row);
            out.i32(cell.column);
        }
    }
    append_queue(out, round.next());
    append_source(out, round.source());
    const auto hole_rng = round.hole_source().rng_state();
    out.u8(hole_rng ? 1 : 0);
    if (hole_rng) out.u64(*hole_rng);
    else out.u8(static_cast<std::uint8_t>(round.hole_cursor()));
    out.i32(round.gravity().elapsed);
    out.i32(round.gravity().interval);
    out.i32(round.soft_drop().remaining);
    out.i32(round.soft_drop().period);
    out.u8(round.rotation_ready() ? 1 : 0);
    out.u64(round.clear_streak());
    out.u64(round.score());
    out.u64(round.total_lines());
    out.u64(round.attack_sent());
    out.i32(round.pending_garbage());
    return out;
}
inline std::optional<std::uint64_t> state_hash(const study_round::Round& round) noexcept {
    return state_bytes(round).digest();
}
} // namespace study_hash
