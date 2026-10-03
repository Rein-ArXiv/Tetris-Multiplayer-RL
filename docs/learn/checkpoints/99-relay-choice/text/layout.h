#pragma once

// Baselines are relative to the first line at y=0. Width is the maximum final
// advance of each line; ink is the separate union of bitmap support rectangles.
// Empty and trailing lines are retained. Zero-area shapes still advance the pen.
// Check valid() before reading result(); rejected edits preserve all state.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace study_layout {

struct Metrics {
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_gap = 0.0f;
};

struct Box {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct Item {
    float pen_x = 0.0f;
    float baseline = 0.0f;
};

struct Result {
    std::array<Item, 16> items{};
    std::size_t count = 0;
    std::array<float, 17> line_widths{};
    std::size_t lines = 1;
    float width = 0.0f;
    float height = 0.0f;
    float line_advance = 0.0f;
    std::optional<Box> ink;
};

class Layout {
public:
    explicit Layout(Metrics metrics) : metrics_(metrics) {}

    // Metrics are well-formed: finite, ascent >= 0, descent <= 0, gap >= 0,
    // ascent - descent > 0, and the line advance is at most 1024.
    bool valid() const {
        if (!std::isfinite(metrics_.ascent) ||
            !std::isfinite(metrics_.descent) ||
            !std::isfinite(metrics_.line_gap)) {
            return false;
        }
        if (metrics_.ascent < 0.0f) return false;
        if (metrics_.descent > 0.0f) return false;
        if (metrics_.line_gap < 0.0f) return false;
        if (!(metrics_.ascent - metrics_.descent > 0.0f)) return false;
        const float advance = line_advance();
        if (!std::isfinite(advance) || advance > 1024.0f) return false;
        return true;
    }

    // True once at least one item has been placed on the current line.
    bool has_previous() const { return previous_; }

    bool add(float advance, float kern_before, Box relative_ink) {
        if (!valid()) return false;
        if (count_ >= 16) return false;

        if (!std::isfinite(advance) || !std::isfinite(kern_before)) return false;
        if (advance < 0.0f || advance > 4096.0f) return false;
        if (std::fabs(kern_before) > 4096.0f) return false;

        // Kerning is only meaningful between two items; the first item of a
        // line must start with a zero kern.
        if (!previous_ && kern_before != 0.0f) return false;

        if (!std::isfinite(relative_ink.x) ||
            !std::isfinite(relative_ink.y) ||
            !std::isfinite(relative_ink.w) ||
            !std::isfinite(relative_ink.h)) {
            return false;
        }
        if (relative_ink.w < 0.0f || relative_ink.w > 4096.0f) return false;
        if (relative_ink.h < 0.0f || relative_ink.h > 4096.0f) return false;
        if (std::fabs(relative_ink.x) > 4096.0f) return false;
        if (std::fabs(relative_ink.y) > 4096.0f) return false;

        // Pen after kerning is the stored position; the new advance pen is that
        // position plus the non-negative advance.
        const float pen_x = pen_ + kern_before;
        const float new_pen = pen_x + advance;
        if (!std::isfinite(pen_x) || !std::isfinite(new_pen)) return false;
        if (new_pen < 0.0f) return false;  // negative total pen

        if (std::fabs(pen_x) > 1000000.0f) return false;
        if (std::fabs(new_pen) > 1000000.0f) return false;

        // Support rectangle in layout space.
        const float box_x = pen_x + relative_ink.x;
        const float box_y = baseline_ + relative_ink.y;
        const float box_x2 = box_x + relative_ink.w;
        const float box_y2 = box_y + relative_ink.h;
        if (!std::isfinite(box_x) || !std::isfinite(box_y) ||
            !std::isfinite(box_x2) || !std::isfinite(box_y2)) {
            return false;
        }
        if (std::fabs(box_x) > 1000000.0f ||
            std::fabs(box_y) > 1000000.0f ||
            std::fabs(box_x2) > 1000000.0f ||
            std::fabs(box_y2) > 1000000.0f) {
            return false;
        }

        Item item;
        item.pen_x = pen_x;
        item.baseline = baseline_;
        items_[count_] = item;
        ++count_;
        previous_ = true;

        pen_ = new_pen;
        line_widths_[lines_ - 1] = pen_;

        // Zero-area support boxes do not extend the ink union.
        if (relative_ink.w > 0.0f && relative_ink.h > 0.0f) {
            const Box placed{box_x, box_y, relative_ink.w, relative_ink.h};
            if (!ink_) {
                ink_ = placed;
            } else {
                const float x0 = std::min(ink_->x, placed.x);
                const float y0 = std::min(ink_->y, placed.y);
                const float x1 = std::max(ink_->x + ink_->w, placed.x + placed.w);
                const float y1 = std::max(ink_->y + ink_->h, placed.y + placed.h);
                ink_ = Box{x0, y0, x1 - x0, y1 - y0};
            }
        }

        return true;
    }

    bool newline() {
        if (!valid()) return false;
        if (lines_ >= 17) return false;

        line_widths_[lines_ - 1] = pen_;
        ++lines_;
        baseline_ += line_advance();
        pen_ = 0.0f;
        previous_ = false;
        return true;
    }

    Result result() const {
        Result out;
        if (!valid()) return out;

        out.items = items_;
        out.count = count_;
        out.line_widths = line_widths_;
        out.lines = lines_;
        out.line_advance = line_advance();
        out.height = metrics_.ascent - metrics_.descent +
                     static_cast<float>(lines_ - 1) * out.line_advance;

        // The widest final line end, including the open current line.
        float widest = 0.0f;
        for (std::size_t i = 0; i < lines_; ++i) {
            widest = std::max(widest, line_widths_[i]);
        }
        out.width = widest;
        out.ink = ink_;
        return out;
    }

private:
    float line_advance() const {
        return metrics_.ascent - metrics_.descent + metrics_.line_gap;
    }

    Metrics metrics_;
    std::array<Item, 16> items_{};
    std::size_t count_ = 0;
    std::array<float, 17> line_widths_{};
    std::size_t lines_ = 1;
    float pen_ = 0.0f;
    float baseline_ = 0.0f;
    bool previous_ = false;
    std::optional<Box> ink_;
};

}  // namespace study_layout
