#ifndef STUDY_NEXT_PIECE_SOURCE_H
#define STUDY_NEXT_PIECE_SOURCE_H

#include <cstddef>
#include <utility>
#include <variant>
#include <type_traits>

#include "simulation/catalog.h"
#include "simulation/scripted_source.h"
#include "simulation/seeded_bag_source.h"

namespace study_next {

// A closed choice of two value-owned source types, stored inline.
// This is a tagged union (variant), not an open type-erasure interface.
class PieceSource {
public:
    using Kind = study_catalog::Kind;

    // Non-explicit: either source converts directly into a PieceSource.
    PieceSource(ScriptedSource source) noexcept : source_(std::move(source)) {}
    PieceSource(SeededBagSource source) noexcept : source_(std::move(source)) {}

    Kind next() noexcept {
        if (auto* scripted = std::get_if<ScriptedSource>(&source_)) {
            return scripted->next();
        }
        return std::get<SeededBagSource>(source_).next();
    }

    std::size_t cursor() const noexcept {
        if (const auto* scripted = std::get_if<ScriptedSource>(&source_)) {
            return scripted->cursor();
        }
        return std::get<SeededBagSource>(source_).cursor();
    }

    // Non-null only for the scripted alternative. Observers cannot mutate it.
    const ScriptedSource* scripted() const noexcept {
        return std::get_if<ScriptedSource>(&source_);
    }

    // Non-null only for the seeded alternative. Observers cannot mutate it.
    const SeededBagSource* seeded() const noexcept {
        return std::get_if<SeededBagSource>(&source_);
    }

private:
    using Storage = std::variant<ScriptedSource, SeededBagSource>;
    // No throwing alternative changes: the fallback std::get is well-defined.
    static_assert(std::is_nothrow_copy_constructible_v<Storage>);
    static_assert(std::is_nothrow_copy_assignable_v<Storage>);
    static_assert(std::is_nothrow_move_constructible_v<Storage>);
    static_assert(std::is_nothrow_move_assignable_v<Storage>);
    Storage source_;
};

}  // namespace study_next

#endif  // STUDY_NEXT_PIECE_SOURCE_H
