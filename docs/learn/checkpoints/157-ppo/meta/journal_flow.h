#ifndef STUDY_META_JOURNAL_FLOW_H
#define STUDY_META_JOURNAL_FLOW_H

#include <cstdint>
#include <optional>
#include <string>

namespace study_meta {

// ---------------------------------------------------------------------------
// Journal flow: single-worker, synchronous orchestration of one pending change.
//
// The store owns a cooperative interprocess lock for its whole lifetime; all
// availability, loading and mutation calls below assume that lock is held.
// The HTTP adapter is supplied separately. Nothing here generates candidates:
// resume_change only replays the request that was already persisted.
//
// No remote rollback and no whole-file-group atomicity is promised. Each disk
// step is an independent durable operation; the recovery file is written before
// the access file so a crash between them leaves a recoverable prefix rather
// than a lost credential.
// ---------------------------------------------------------------------------

struct PendingChange {
    std::string origin;
    std::string operation;
    std::string credential;
    std::string next_token;
    std::string next_recovery;
    std::uint64_t expected_player_id = 0;
};

enum class JournalRead { missing, ready, blocked };

struct LoadedChange {
    JournalRead state = JournalRead::blocked;
    PendingChange request;
};

enum class RemoteState { accepted, rejected, uncertain };

struct RemoteChange {
    RemoteState state = RemoteState::uncertain;
    std::uint64_t player_id = 0;
    std::uint64_t epoch = 0;
};

enum class JournalState {
    idle,
    blocked,
    pending,
    rejected,
    local_incomplete,
    complete
};

struct JournalResult {
    JournalState state = JournalState::blocked;
    std::uint64_t player_id = 0;
};

template <class Store, class Api>
JournalResult resume_change(Store& store, Api& api)
{
    // Nothing is loaded yet, so a failure here is a hard block.
    bool have_pending = false;
    try {
        if (!store.available() || !api.valid())
            return {JournalState::blocked, 0};

        const std::string origin = store.origin();
        if (origin != api.origin())
            return {JournalState::blocked, 0};

        // load_pending() enforces strict format, the exact origin, semantic
        // validation and ownership of all credential files; a ready record
        // still has to name our origin before we trust it.
        LoadedChange loaded = store.load_pending();
        if (loaded.state == JournalRead::missing)
            return {JournalState::idle, 0};
        if (loaded.state != JournalRead::ready)
            return {JournalState::blocked, 0};
        if (loaded.request.origin != origin)
            return {JournalState::blocked, 0};

        have_pending = true;
        const PendingChange request = loaded.request;

        // Replay the stored request exactly once; no candidates are generated.
        RemoteChange remote = api.change(request);

        if (remote.state == RemoteState::rejected) {
            // A durable journal clear turns rejection into a terminal local state.
            try {
                if (store.remove_pending())
                    return {JournalState::rejected, 0};
            } catch (...) {
            }
            return {JournalState::pending, 0};
        }

        if (remote.state != RemoteState::accepted)
            return {JournalState::pending, 0};

        // Remote accepted. That is only a claim about the far side, not proof
        // of local durability, so it is never reported as complete directly.
        //
        // Match the intended account before publishing local credentials.
        // ID equality is a consistency check, not authentication of the server.
        // TLS and request/response binding remain transport responsibilities.
        if (remote.player_id == 0 || remote.player_id != request.expected_player_id)
            return {JournalState::pending, 0};

        // Disk completion, in order: recovery, then access, then pending
        // clear. The journal is cleared only after both active files are saved.
        // A writer may report failure after replacement; re-read on restart.
        try {
            if (!store.save_recovery(request, remote.player_id))
                return {JournalState::local_incomplete, 0};
            if (!store.save_access(request, remote.player_id))
                return {JournalState::local_incomplete, 0};
            if (!store.remove_pending())
                return {JournalState::local_incomplete, 0};
        } catch (...) {
            return {JournalState::local_incomplete, 0};
        }

        // Only now is the change complete, and only now is the id returned.
        return {JournalState::complete, remote.player_id};
    } catch (...) {
        // Before acceptance the journal is never deleted here: a valid pending
        // change degrades to pending, anything earlier stays blocked. The
        // accepted path above already returned local_incomplete on failure.
        return {have_pending ? JournalState::pending : JournalState::blocked, 0};
    }
}

} // namespace study_meta

#endif // STUDY_META_JOURNAL_FLOW_H
