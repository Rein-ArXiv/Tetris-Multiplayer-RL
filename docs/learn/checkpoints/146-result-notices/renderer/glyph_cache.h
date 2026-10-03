#pragma once

// study_text: GPU-side glyph cache.
//
// Maps study_font::CachedGlyph snapshots to regions in a borrowed
// study_atlas::GlyphAtlas. The atlas must outlive this cache. Render-thread
// only; there are no callbacks and no synchronization.
//
// Uploads are append-only: a successful insert stays in the atlas until the
// caller clears the atlas itself. Clearing this cache's lookup entries does
// not reclaim shelf space or texture pages.
//
// Each entry holds its own shared_ptr, which keeps the allocation alive and
// makes pointer identity a stable lookup key: a later glyph allocation cannot
// reuse the address while the entry is present.

#include "renderer/glyph_atlas.h"  // study_atlas::GlyphAtlas, Region
#include "text/glyph_cache.h"      // study_font::CachedGlyph

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace study_text {

using Region = study_atlas::Region;

class GpuGlyphCache final {
public:
    // Borrows atlas; it must outlive this cache.
    explicit GpuGlyphCache(study_atlas::GlyphAtlas& atlas) noexcept : atlas_(atlas) {}

    GpuGlyphCache(const GpuGlyphCache&) = delete;
    GpuGlyphCache& operator=(const GpuGlyphCache&) = delete;
    GpuGlyphCache(GpuGlyphCache&&) = delete;
    GpuGlyphCache& operator=(GpuGlyphCache&&) = delete;

    // Returns the atlas region for glyph, inserting it when absent.
    //
    // A hit is decided by shared_ptr allocation identity and performs no GL
    // work. If the atlas revision changed, every lookup entry is dropped first
    // because old regions can no longer be trusted. A full cache refuses before
    // any upload. A failed insert retains nothing, so a later call can retry.
    std::optional<Region> get(
            const std::shared_ptr<const study_font::CachedGlyph>& glyph) {
        const std::uint64_t revision = atlas_.revision();

        // Regions are only meaningful for the revision that produced them.
        if (revision != cached_revision_) {
            release_lookup();
            cached_revision_ = revision;
        }

        if (revision == 0 || !glyph) {
            return std::nullopt;
        }

        // Identity lookup: the stored shared_ptr keeps the allocation alive, so
        // its address cannot be recycled while the entry exists.
        for (std::size_t i = 0; i < count_; ++i) {
            if (entries_[i].glyph == glyph) {
                return entries_[i].region;
            }
        }

        if (count_ >= capacity) {
            return std::nullopt;  // full: refuse before uploading
        }

        // Failed insertion publishes no Region or shelf reservation. A GPU
        // failure may have touched unused pixels; retry rewrites the whole slot.
        const std::optional<Region> region = atlas_.insert(glyph->bitmap);
        if (!region) {
            return std::nullopt;
        }

        // Commit only after the atlas accepted and wrote the bitmap.
        entries_[count_].glyph = glyph;
        entries_[count_].region = *region;
        ++count_;
        return region;
    }

    // Releases this cache's lookup entries and records the current atlas
    // revision. It does not reclaim shelf or texture storage; successful
    // uploads remain until the caller clears the atlas itself.
    void clear() {
        release_lookup();
        cached_revision_ = atlas_.revision();
    }

    // Number of entries this cache currently owns. It may be stale with respect
    // to a revision change until the next get().
    std::size_t size() const noexcept { return count_; }

    // The borrowed atlas, for callers that prepare text against it.
    const study_atlas::GlyphAtlas& atlas() const noexcept { return atlas_; }

private:
    static constexpr std::size_t capacity = 64;

    struct Entry {
        std::shared_ptr<const study_font::CachedGlyph> glyph;
        Region region{};
    };

    void release_lookup() noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            entries_[i].glyph.reset();
        }
        count_ = 0;
    }

    study_atlas::GlyphAtlas& atlas_;
    std::array<Entry, capacity> entries_{};
    std::size_t count_ = 0;
    std::uint64_t cached_revision_ = 0;
};

}  // namespace study_text
