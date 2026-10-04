#include "simulation/catalog.h"
#include "simulation/movement.h"
#include <climits>
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "movement line %d: %s\n", __LINE__, #x); return 1; } } while (false)
using study_movement::Result;
static bool same(const study_piece::Piece& a, const study_piece::Piece& b) {
    if (a.origin.row != b.origin.row || a.origin.column != b.origin.column) return false;
    for (std::size_t i = 0; i < a.local.size(); ++i) {
        if (a.local[i].row != b.local[i].row || a.local[i].column != b.local[i].column) return false;
    }
    return true;
}
// Independent scalar oracle for the modest exhaustive range below.
static bool inside(const study_piece::Piece& p, int dc = 0) {
    for (auto c : p.local) {
        const int r = c.row + p.origin.row, col = c.column + p.origin.column + dc;
        if (r < 0 || r >= 20 || col < 0 || col >= 10) return false;
    }
    return true;
}
int main() {
    CHECK(study_movement::horizontal_intent(false, false) == 0);
    CHECK(study_movement::horizontal_intent(true, true) == 0);
    CHECK(study_movement::horizontal_intent(true, false) == -1);
    CHECK(study_movement::horizontal_intent(false, true) == 1);
    int cases = 0;
    for (const auto& definition : study_catalog::definitions) {
        for (int r = -2; r <= 21; ++r) for (int c = -4; c <= 11; ++c) {
            for (int step = -2; step <= 2; ++step) {
                auto piece = *study_catalog::make_piece(definition.kind);
                piece.origin = {r, c};
                const auto before = piece;
                auto expected = Result::invalid;
                if (step >= -1 && step <= 1 && inside(before)) {
                    expected = step == 0 ? Result::idle : inside(before, step) ? Result::moved : Result::blocked;
                }
                CHECK(study_movement::try_shift(piece, step) == expected);
                auto target = before;
                if (expected == Result::moved) target.origin.column += step;
                CHECK(same(piece, target));
                ++cases;
            }
        }
    }
    auto piece = *study_catalog::make_piece(study_catalog::Kind::T);
    const auto before = piece;
    for (int step : {INT_MIN, INT_MAX}) {
        CHECK(study_movement::try_shift(piece, step) == Result::invalid);
        CHECK(same(piece, before));
    }
    // Origin outside board can still describe valid occupied cells.
    for (auto& c : piece.local) c.column += 100;
    piece.origin.column -= 100;
    CHECK(study_movement::inside_board(piece));
    CHECK(study_movement::try_shift(piece, -1) == Result::moved);
    CHECK(piece.origin.column == -98);
    // A representable board position with an unrepresentable next origin.
    piece.origin = {0, INT_MAX};
    piece.local = {{{0,-INT_MAX},{0,-INT_MAX+1},{1,-INT_MAX},{1,-INT_MAX+1}}};
    const auto extreme = piece;
    CHECK(study_movement::inside_board(piece));
    CHECK(study_movement::try_shift(piece, 1) == Result::invalid);
    CHECK(same(piece, extreme));
    piece.origin = {INT_MAX, 0};
    piece.local = study_piece::t_shape;
    const auto bad = piece;
    CHECK(study_movement::try_shift(piece, 0) == Result::invalid);
    CHECK(same(piece, bad));
    // Occupancy is explicitly absent from this boundary-only movement layer.
    study_grid::Grid board;
    CHECK(board.set(0, 3, study_grid::Cell::filled));
    piece = *study_catalog::make_piece(study_catalog::Kind::T);
    const auto board_before = board.cells();
    CHECK(study_movement::try_shift(piece, -1) == Result::moved);
    CHECK(board.cells() == board_before);
    std::printf("movement: %d state/step cases, intent, rejected state preservation and extreme origins passed\n", cases);
}
