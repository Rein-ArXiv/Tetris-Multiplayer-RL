#include "gl_api.h"

#include <cstdio>

namespace study_gl {
namespace {

// Resolves one slot. `Fn` is deduced as the concrete function-pointer type
// (never a reference), so the cast target is exact and keeps its convention.
template <typename Fn>
bool resolve_slot(Fn& slot, Resolver resolver, const char* name)
{
    void* raw = resolver(name);
    if (raw == nullptr) {
        // Report this name and keep going, so the caller sees every gap.
        std::fprintf(stderr, "[GL] missing entry point: %s\n", name);
        return false;
    }
    // `Fn` is the value type of the slot. The void* -> function-pointer conversion is the
    // platform (SDL) contract, not a universally portable ISO rule.
    slot = reinterpret_cast<Fn>(raw);
    return true;
}

} // namespace

bool load(GlApi& out, Resolver resolver)
{
    // Clear first: a previous successful load must not survive a new attempt.
    out = GlApi{};

    if (resolver == nullptr) {
        std::fprintf(stderr, "[GL] load called with a null resolver.\n");
        return false;
    }

    // Fill a local candidate. `out` stays cleared unless every slot resolves.
    GlApi candidate;
    bool ok = true;
    ok = resolve_slot(candidate.GetString,   resolver, "glGetString") && ok;
    ok = resolve_slot(candidate.GetIntegerv, resolver, "glGetIntegerv") && ok;
    ok = resolve_slot(candidate.GetError,    resolver, "glGetError") && ok;

    if (!ok) {
        return false; // candidate is discarded; `out` remains cleared.
    }

    out = candidate; // all slots valid: publish at once.
    return true;
}

} // namespace study_gl
