#pragma once
// One geometry basis for input and drawing. Changes cancel this frame's pointer.
// Sampling is not OS-atomic; resize events must also cancel changes that return
// to the same dimensions inside one event pump. Keyboard input is independent.
#include <optional>

#include "immediate_ui.h"
#include "renderer/letterbox.h"
#include "platform/pointer_edges.h"

namespace study_ui {

// One frame of results. layout is absent when the sizes are degenerate; it is
// still reported on a geometry change so the frame can be drawn.
struct MappingFrame {
    std::optional<study_letterbox::Layout> layout;
    Input input;
    bool geometry_changed = false;
};

class FrameMapping final {
public:
    // Consume one frame of geometry and native pointer state. The layout is
    // built exactly once and reused for pointer mapping. Dimensions are always
    // stored, even when invalid, so invalid -> same invalid is seen as
    // unchanged on the next call.
    MappingFrame update(study_letterbox::Size window,
                        study_letterbox::Size drawable,
                        study_letterbox::Size logical,
                        const study_pointer::Frame& frame) noexcept {
        MappingFrame out;

        // First call has no previous snapshot, so it counts as a change.
        bool changed = true;
        if (previous_) {
            const Dimensions& p = *previous_;
            changed = p.window.width != window.width ||
                      p.window.height != window.height ||
                      p.drawable.width != drawable.width ||
                      p.drawable.height != drawable.height ||
                      p.logical.width != logical.width ||
                      p.logical.height != logical.height;
        }

        // Always save the snapshot, valid or not.
        previous_ = Dimensions{window, drawable, logical};

        out.geometry_changed = changed;

        // Build the layout once and reuse it for pointer mapping.
        out.layout = study_letterbox::make_layout(window, drawable, logical);
        out.input = map_pointer(out.layout, frame);

        // A resize drops the pointer for this frame, but the layout stays for
        // drawing. Only mouse input is touched; keyboard is unaffected.
        if (changed) {
            out.input = Input{};
            out.input.cancelled = true;
        }
        return out;
    }

private:
    struct Dimensions {
        study_letterbox::Size window, drawable, logical;
    };
    std::optional<Dimensions> previous_;
};

}  // namespace study_ui
