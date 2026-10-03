#pragma once

// Static catalog of canonical tetromino spawn definitions.
// Immutable data and CPU helpers: no heap, GL/SDL, RNG, or bag logic.

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "simulation/piece.h"

namespace study_catalog {

using study_piece::LocalCell;
using study_piece::Origin;
using study_piece::Piece;
using study_piece::Shape;

enum class Kind : std::uint8_t {
    L = 1,
    J = 2,
    I = 3,
    O = 4,
    S = 5,
    T = 6,
    Z = 7,
};

struct Definition {
    Kind kind;
    std::string_view name;
    Shape cells;
    Origin spawn;
};

using Catalog = std::array<Definition, 7>;

// Catalog order is I, J, L, O, S, T, Z (current game factory order), not Kind ID order.
inline constexpr Catalog definitions{{
    {Kind::I, "I", {{{1, 0}, {1, 1}, {1, 2}, {1, 3}}}, {0, 3}},
    {Kind::J, "J", {{{0, 0}, {1, 0}, {1, 1}, {1, 2}}}, {0, 3}},
    {Kind::L, "L", {{{0, 2}, {1, 0}, {1, 1}, {1, 2}}}, {0, 3}},
    {Kind::O, "O", {{{0, 0}, {0, 1}, {1, 0}, {1, 1}}}, {0, 4}},
    {Kind::S, "S", {{{0, 1}, {0, 2}, {1, 0}, {1, 1}}}, {0, 3}},
    {Kind::T, "T", study_piece::t_shape, {0, 3}},
    {Kind::Z, "Z", {{{0, 0}, {0, 1}, {1, 1}, {1, 2}}}, {0, 3}},
}};

// Structural check: in-range cells, no duplicate cells, edge-connected from 0.
// No rotation/mirror normalization is performed.
constexpr bool valid_shape(const Shape& shape) noexcept {
    for (const LocalCell& cell : shape) {
        if (cell.row < 0 || cell.row > 3 || cell.column < 0 || cell.column > 3) {
            return false;
        }
    }

    for (std::size_t i = 0; i < shape.size(); ++i) {
        for (std::size_t j = i + 1; j < shape.size(); ++j) {
            if (shape[i].row == shape[j].row && shape[i].column == shape[j].column) {
                return false;
            }
        }
    }

    bool reached[4] = {false, false, false, false};
    reached[0] = true;
    for (int pass = 0; pass < 4; ++pass) {
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (!reached[i]) {
                continue;
            }
            for (std::size_t j = 0; j < shape.size(); ++j) {
                if (reached[j]) {
                    continue;
                }
                const int dr = shape[i].row - shape[j].row;
                const int dc = shape[i].column - shape[j].column;
                const int adr = dr < 0 ? -dr : dr;
                const int adc = dc < 0 ? -dc : dc;
                if (adr + adc == 1) {
                    reached[j] = true;
                }
            }
        }
    }

    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (!reached[i]) {
            return false;
        }
    }
    return true;
}

// Validates IDs/names, shape structure, spawn and absolute cell bounds.
// Bounds describe initial placement policy only, not collision/game-over.
constexpr bool valid_catalog(const Catalog& catalog) noexcept {
    for (std::size_t i = 0; i < catalog.size(); ++i) {
        const Definition& record = catalog[i];
        const int id = static_cast<int>(record.kind);
        if (id < 1 || id > 7) {
            return false;
        }
        if (record.name.size() != 1) {
            return false;
        }
        const char letter = record.name[0];
        if (letter < 'A' || letter > 'Z') {
            return false;
        }
        if (!valid_shape(record.cells)) {
            return false;
        }

        const std::int64_t spawnRow = record.spawn.row;
        const std::int64_t spawnColumn = record.spawn.column;
        if (spawnRow < 0 || spawnRow >= 20 || spawnColumn < 0 || spawnColumn >= 10) {
            return false;
        }
        for (const LocalCell& cell : record.cells) {
            const std::int64_t row = static_cast<std::int64_t>(cell.row) + spawnRow;
            const std::int64_t column = static_cast<std::int64_t>(cell.column) + spawnColumn;
            if (row < 0 || row >= 20 || column < 0 || column >= 10) {
                return false;
            }
        }

        for (std::size_t j = i + 1; j < catalog.size(); ++j) {
            if (record.kind == catalog[j].kind || record.name == catalog[j].name) {
                return false;
            }
        }
    }
    return true;
}

static_assert(valid_catalog(definitions), "catalog must be structurally valid");

// Identity lookups; definitions stays immutable and pointers are borrowed.
constexpr const Definition* find(Kind kind) noexcept {
    for (const Definition& record : definitions) {
        if (record.kind == kind) {
            return &record;
        }
    }
    return nullptr;
}

// Compares the raw int to the stored ID; input is never narrowed to Kind.
constexpr const Definition* find_id(int id) noexcept {
    for (const Definition& record : definitions) {
        if (static_cast<int>(record.kind) == id) {
            return &record;
        }
    }
    return nullptr;
}

// Exact, case-sensitive name match; no whitespace or case normalization.
constexpr const Definition* find_name(std::string_view name) noexcept {
    for (const Definition& record : definitions) {
        if (record.name == name) {
            return &record;
        }
    }
    return nullptr;
}

// Returns an independent mutable Piece copy; unknown kinds yield nullopt.
constexpr std::optional<Piece> make_piece(Kind kind) noexcept {
    const Definition* definition = find(kind);
    if (definition == nullptr) {
        return std::nullopt;
    }
    return Piece{definition->cells, definition->spawn};
}

}  // namespace study_catalog
