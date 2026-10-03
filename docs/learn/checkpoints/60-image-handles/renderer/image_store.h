#pragma once
#include "renderer/handle_pool.h"
#include "renderer/image_decode.h"
#include "renderer/texture_quad.h"
#include <memory>
#include <new>
namespace study_image {
using Handle=study_handles::Handle;
// Single render thread; the current GL context and GlApi outlive this store.
// Finish any CPU queues using these textures before unload/clear/destruction.
class ImageStore {
public:
    explicit ImageStore(const study_gl::GlApi& gl) noexcept : gl_(gl) {}
    ImageStore(const ImageStore&)=delete;
    ImageStore& operator=(const ImageStore&)=delete;
    Handle create(study_texture::RgbaView pixels) noexcept {
        if(!study_texture::valid(pixels)) return 0;
        try {
            auto texture=std::make_unique<study_texture::Texture>(gl_);
            if(!texture->upload(pixels)) return 0;
            return images_.insert(std::move(texture));
        } catch(const std::bad_alloc&) { return 0; }
    }
    Handle load(const std::filesystem::path& path) noexcept {
        const auto decoded=decode_file(path);
        return decoded ? create(decoded.image->view()) : 0;
    }
    bool size(Handle handle,int& width,int& height) const noexcept {
        const auto* image=images_.find(handle);
        if(!image) return false;
        width=image->width();height=image->height();return true;
    }
    bool draw(study_texture::Quad& quad,Handle handle) const noexcept {
        const auto* image=images_.find(handle);
        return image && quad.draw(*image);
    }
    bool unload(Handle handle) noexcept { return images_.erase(handle); }
    void clear() noexcept { images_.clear(); }
    std::size_t count() const noexcept { return images_.size(); }
private:
    const study_gl::GlApi& gl_;
    study_handles::HandlePool<study_texture::Texture> images_;
};
} // namespace study_image
