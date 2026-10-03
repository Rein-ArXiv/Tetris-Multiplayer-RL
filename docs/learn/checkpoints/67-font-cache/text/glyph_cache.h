#pragma once

// study_font: CPU glyph cache for a teaching checkpoint.
//
// TRUSTED ASSETS ONLY. This header rasterizes only fonts loaded through
// Font::load_trusted. It performs no byte-level validation of its own; the
// Font contract already covers the trusted-asset requirement.
//
// Threading: render-thread only, not thread-safe. All methods mutate or read
// cache state without synchronization and must be called from one thread.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <utility>

#include "font.h"                 // Font, Glyph, Metrics, HorizontalMetrics
#include "raster_policy.h"   // font_raster::plan

namespace study_font {

// One immutable cached glyph snapshot.
//
// The device-pixel bitmap and its device-unit advance/bearing live in `bitmap`.
// The logical advance/bearing are stored separately so the layout remains in logical units. Bitmap rectangles
// are converted back to logical units before the viewport transform.
struct CachedGlyph {
    Glyph bitmap;               // coverage plus device-pixel metrics
    float advance{};            // horizontal advance in logical pixels
    float left_bearing{};       // left side bearing in logical pixels
    float logical_per_device{}; // logical pixels per device pixel for this entry
    std::uint64_t key{};        // packed identity produced by font_raster::plan
};

// Lifetime counters for the cache. They are never reset by clear() or
// load_trusted(); they describe everything the cache has done since creation.
struct CacheStats {
    std::size_t hits = 0;            // successful get() lookups
    std::size_t raster_attempts = 0; // calls issued to Font::rasterize_device
};

// Fixed-capacity, render-thread-only glyph cache.
//
// The cache privately owns exactly one Font. Because a successful reload removes all lookup entries
// and no mutable Font is exposed, the packed key does not need to encode a font pointer or generation.
//
// Capacity bounds the number of entries the cache owns, not the number of live
// glyphs in the process: callers may retain the returned shared_ptr snapshots
// after the cache has been cleared or has released them.
//
// The allocation identity carried by each returned shared_ptr can also key a
// GPU-side cache, but only if the GPU cache holds its own shared_ptr too; that
// keeps the allocation alive and prevents a later allocation from reusing the
// same address.
class GlyphCache final {
public:
    // Maximum number of entries the cache owns at once.
    static constexpr std::size_t capacity = 64;

    GlyphCache() = default;
    ~GlyphCache() = default;

    GlyphCache(const GlyphCache&) = delete;
    GlyphCache& operator=(const GlyphCache&) = delete;
    GlyphCache(GlyphCache&&) = delete;
    GlyphCache& operator=(GlyphCache&&) = delete;

    // Loads a trusted packaged font.
    //
    // On failure the cache and its stats are unchanged. On success every lookup
    // entry is released, even when reloading the same path, because the
    // new font may produce different glyph data. Snapshots already returned to
    // callers stay valid: they are immutable and shared-owned.
    bool load_trusted(const std::filesystem::path& path) {
        if (!font_.load_trusted(path)) {
            return false; // preserve existing entries
        }
        clear();
        return true;
    }

    // Releases only the entries this cache owns. Snapshots still held elsewhere
    // (for example a GPU cache that keeps the same shared_ptr) remain valid.
    void clear() {
        for (auto& slot : entries_) {
            slot.reset();
        }
    }

    // Number of entries currently owned by the cache.
    std::size_t size() const noexcept {
        std::size_t count = 0;
        for (const auto& slot : entries_) {
            if (slot) {
                ++count;
            }
        }
        return count;
    }

    // Returns a cached glyph for the requested logical size and render scale.
    //
    // The lookup key is built by font_raster::plan from (scalar, logical height,
    // integer device height), so identical requests share an entry and
    // the same shared_ptr is returned on a hit. A miss runs a two-stage font
    // query: horizontal() for logical metrics and rasterize_device() for
    // physical pixels. If either stage fails, no entry is added.
    //
    // Returns nullptr when the request is out of range, when font_raster::plan
    // rejects it, when the cache is full, or when a font query fails.
    std::shared_ptr<const CachedGlyph> get(char32_t scalar,
                                           int logical_height,
                                           double render_scale) {
        // Stricter than the shared policy's 2048 ceiling: this cache only serves
        // the same logical size domain as Font::metrics.
        if (logical_height < 1 || logical_height > kMaxLogicalHeight) {
            return nullptr;
        }

        const std::optional<font_raster::Plan> plan =
            font_raster::plan(static_cast<std::uint32_t>(scalar),
                              logical_height, render_scale);
        if (!plan) {
            return nullptr;
        }

        // Linear scan of at most 64 slots keeps this example allocation-free
        // on lookup. A larger cache would need a measured indexing policy.
        for (const auto& slot : entries_) {
            if (slot && slot->key == plan->key) {
                ++stats_.hits;
                return slot; // identical shared_ptr: same allocation identity
            }
        }

        // Full cache: refuse before doing any rasterization work. This cache has
        // no replacement policy; the caller decides what to release.
        if (size() >= capacity) {
            return nullptr;
        }

        return emplace(scalar, *plan);
    }

    // Lifetime statistics, returned by value.
    CacheStats stats() const noexcept { return stats_; }

    // --- Paragraph preparation delegates (never populate the cache) ---

    std::optional<Metrics> metrics(float pixel_height) const {
        return font_.metrics(pixel_height);
    }

    std::optional<float> kerning(int left, int right, float pixel_height) const {
        return font_.kerning(left, right, pixel_height);
    }

private:
    // The Font contract caps pixel heights at 128. This cache serves UI text and
    // restates the same bound here so the intent is visible locally.
    static constexpr int kMaxLogicalHeight = 128;

    std::size_t first_free_slot() const noexcept {
        for (std::size_t i = 0; i < capacity; ++i) {
            if (!entries_[i]) {
                return i;
            }
        }
        return capacity;
    }

    // Builds and stores one entry. Nothing is committed unless the whole entry
    // is constructed successfully.
    std::shared_ptr<const CachedGlyph> emplace(char32_t scalar,
                                               const font_raster::Plan& plan) {
        // Stage 1: logical metrics, no pixels. A failure means no entry.
        const std::optional<HorizontalMetrics> horizontal =
            font_.horizontal(scalar, static_cast<float>(plan.logical_height));
        if (!horizontal) {
            return nullptr;
        }

        // Stage 2: physical pixels. Count the attempt immediately before the
        // call so even a failing attempt is recorded.
        ++stats_.raster_attempts;
        std::optional<Glyph> bitmap =
            font_.rasterize_device(scalar, plan.device_height);
        if (!bitmap) {
            return nullptr;
        }

        // Convert the metric height ratio to a single float for this entry.
        const float logical_per_device =
            static_cast<float>(plan.logical_height) /
            static_cast<float>(plan.device_height);

        // Build the immutable entry. Allocation failure is reported as a miss
        // rather than an exception so the cache stays usable under pressure.
        std::shared_ptr<const CachedGlyph> entry;
        try {
            entry = std::make_shared<const CachedGlyph>(CachedGlyph{
                std::move(*bitmap),
                horizontal->advance,
                horizontal->left_bearing,
                logical_per_device,
                plan.key,
            });
        } catch (const std::bad_alloc&) {
            return nullptr;
        }

        // Commit only after the shared_ptr exists, so a failed construction can
        // never leave a partial slot behind or inflate the owned count.
        const std::size_t slot = first_free_slot();
        if (slot == capacity) {
            return nullptr; // defensive; get() already checked for space
        }
        entries_[slot] = entry;
        return entry;
    }

    Font font_;                                                     // privately owned
    std::array<std::shared_ptr<const CachedGlyph>, capacity> entries_{};
    CacheStats stats_{};
};

} // namespace study_font
