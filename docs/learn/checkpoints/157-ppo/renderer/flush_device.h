#pragma once
#include "renderer/batch_device.h"
#include "renderer/clip_box.h"
namespace study_flush {
// The frame's letterbox pass has enabled scissor and selected the viewport.
struct GlSink {
    const study_gl::GlApi& gl;
    study_batch::Device& device;
    bool set_clip(Clip clip) noexcept {
        if (!valid(clip) || gl.GetError()) return false;
        gl.Scissor(clip.x,clip.y,clip.width,clip.height);
        return gl.GetError()==0;
    }
    bool submit(const study_batch::Batch& batch) noexcept { return device.submit(batch); }
};
} // namespace study_flush
