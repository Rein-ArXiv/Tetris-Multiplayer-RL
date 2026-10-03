#ifndef STUDY_ATLAS_SHELF_H
#define STUDY_ATLAS_SHELF_H

// Left-to-right shelves; each returned half-open outer rectangle reserves
// a one-pixel border around ink. The uploader must fill that border with zero.
// Rejected insertions preserve all cursor state. Copy the Shelf to plan an
// upload, and commit the copy only after the upload succeeds.

#include <optional>

namespace study_atlas {

class Shelf {
public:
    // Half-open pixel rectangle; see the note above.
    struct Rect {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    // A reserved atlas region: outer includes the zero border, ink is the
    // actual glyph area.
    struct Placement {
        Rect outer;
        Rect ink;
    };

    // Smallest and largest atlas edge length accepted by valid().
    static constexpr int kMinDimension = 3;
    static constexpr int kMaxDimension = 4096;

    explicit Shelf(int width, int height)
        : width_(width), height_(height) {}

    int width() const { return width_; }
    int height() const { return height_; }

    // True when the atlas dimensions are usable. An out-of-range Shelf simply
    // rejects every insert instead of throwing.
    bool valid() const {
        return width_ >= kMinDimension && width_ <= kMaxDimension &&
               height_ >= kMinDimension && height_ <= kMaxDimension;
    }

    // Reserve a cell for a non-empty glyph. Returns std::nullopt when the
    // input is not a positive glyph that fits with its border, when the atlas
    // is invalid, or when no room remains. On success the cursor advances.
    std::optional<Placement> insert(int ink_width, int ink_height) {
        if (!valid()) {
            return std::nullopt;
        }
        if (ink_width <= 0 || ink_height <= 0) {
            return std::nullopt;
        }
        // Bounds are checked by subtraction so no addition can overflow and so
        // the exact remaining space is compared. The +2 border is added only
        // after the ink size itself is known to fit.
        if (ink_width > width_ - 2 || ink_height > height_ - 2) {
            return std::nullopt;
        }

        const int outer_width = ink_width + 2;
        const int outer_height = ink_height + 2;

        // Candidate placement, computed without touching the members.
        int candidate_x = cursor_x_;
        int candidate_y = cursor_y_;
        int candidate_row_height = row_height_;

        // Not enough room on the current shelf: wrap to a fresh shelf on the
        // next row. The wrap itself is only a candidate change.
        if (outer_width > width_ - candidate_x) {
            candidate_x = 0;
            candidate_y += row_height_;
            candidate_row_height = 0;
        }

        // The glyph must fit horizontally at the candidate x and vertically at
        // the candidate y. Both are subtraction comparisons.
        if (outer_width > width_ - candidate_x) {
            return std::nullopt;
        }
        if (outer_height > height_ - candidate_y) {
            return std::nullopt;
        }

        // All checks passed: commit the placement.
        Placement placement;
        placement.outer.x = candidate_x;
        placement.outer.y = candidate_y;
        placement.outer.w = outer_width;
        placement.outer.h = outer_height;
        placement.ink.x = candidate_x + 1;
        placement.ink.y = candidate_y + 1;
        placement.ink.w = ink_width;
        placement.ink.h = ink_height;

        cursor_x_ = placement.outer.x + placement.outer.w;
        cursor_y_ = placement.outer.y;
        row_height_ =
            candidate_row_height > outer_height ? candidate_row_height : outer_height;

        return placement;
    }

    // Reset the cursor so the atlas can be reused. Dimensions are preserved.
    void clear() noexcept {
        cursor_x_ = 0;
        cursor_y_ = 0;
        row_height_ = 0;
    }

private:
    int width_ = 0;
    int height_ = 0;
    int cursor_x_ = 0;
    int cursor_y_ = 0;
    int row_height_ = 0;
};

} // namespace study_atlas

#endif // STUDY_ATLAS_SHELF_H
