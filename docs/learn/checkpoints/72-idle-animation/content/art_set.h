#pragma once
#include "content/characters.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace study_art {

using Handle = std::uint64_t;

struct Art {
    Handle icon = 0;
    Handle portrait = 0;
    bool icon_fallback = false;
    bool portrait_fallback = false;
};

// Borrows opaque texture handles owned by an ImageStore. Stores no GL objects
// and has no destructor that unloads; the store/context must outlive this set
// and must not be cleared/unloaded while these handles are in use.
class ArtSet {
public:
    // Resolves every role in the catalog. One loader attempt per distinct,
    // non-empty path (failures cached as 0); empty/null paths skip the loader
    // and use `fallback`. Returns false if already initialized or if
    // `fallback` is 0. Loader exceptions propagate: any external resources the
    // loader already acquired stay the loader's responsibility, not rolled
    // back here.
    template <class Load>
    bool init(Load&& load, Handle fallback) {
        if (ready_) return false;
        if (fallback == 0) return false;

        std::unordered_map<std::string, Handle> cache;
        std::array<Art, study_characters::characters.size()> candidate{};

        for (std::size_t i = 0; i < study_characters::characters.size(); ++i) {
            const study_characters::Character& character =
                study_characters::characters[i];
            Art& art = candidate[i];
            assign_role(load, cache, character.icon_path, fallback,
                        art.icon, art.icon_fallback);
            assign_role(load, cache, character.portrait_path, fallback,
                        art.portrait, art.portrait_fallback);
        }

        arts_ = candidate;
        ready_ = true;
        return true;
    }

    // Borrowed art for a known id; nullptr when unknown or not initialized.
    const Art* resolve(std::string_view id) const noexcept {
        if (!ready_) return nullptr;
        const std::optional<std::size_t> index = study_characters::find_index(id);
        if (!index) return nullptr;
        return &arts_[*index];
    }

    bool ready() const noexcept { return ready_; }

private:
    template <class Load>
    static void assign_role(Load& load,
                            std::unordered_map<std::string, Handle>& cache,
                            const char* path, Handle fallback,
                            Handle& out, bool& out_fallback) {
        const Handle handle = acquire(load, cache, path);
        if (handle != 0) {
            out = handle;
            out_fallback = false;
        } else {
            out = fallback;
            out_fallback = true;
        }
    }

    template <class Load>
    static Handle acquire(Load& load,
                          std::unordered_map<std::string, Handle>& cache,
                          const char* path) {
        if (path == nullptr || path[0] == '\0') return 0;
        const std::string key(path);
        const auto found = cache.find(key);
        if (found != cache.end()) return found->second;
        const Handle handle = load(key); // loader takes const std::string&
        cache.emplace(key, handle);
        return handle;
    }

    std::array<Art, study_characters::characters.size()> arts_{};
    bool ready_ = false;
};

} // namespace study_art
