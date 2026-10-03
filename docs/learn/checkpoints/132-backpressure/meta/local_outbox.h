#pragma once
#include "meta/account_file_lock.h"
#include "meta/local_document.h"
#include "meta/outbox_wire.h"
#include "settings/private_file.h"
#include <utility>
namespace study_meta {
enum class OutboxLoad { missing, ready, blocked };
struct LoadedOutbox {
    OutboxLoad state = OutboxLoad::blocked;
    std::optional<OutboxDocument> document;
};
// One folder holds one immutable operation. Confirmed receipts remain on disk.
class LocalOutbox {
public:
    using Writer = bool (*)(const std::string&, const std::string&);
    LocalOutbox(std::filesystem::path folder, std::string origin,
                Writer writer = study_files::write_private_file)
        : origin_(std::move(origin)), file_(folder / "outbox.json"),
          lock_((folder / "outbox.lock").u8string()), writer_(writer) {}
    bool available() const { return lock_.locked() && writer_ && !origin_.empty(); }
    const std::string& origin() const { return origin_; }
    LoadedOutbox load() const {
        if (!available()) return {};
        const auto raw = read_local_document(file_);
        if (raw.state == LocalRead::missing) return {OutboxLoad::missing, {}};
        if (raw.state != LocalRead::ready) return {};
        const auto doc = parse_outbox(raw.body);
        if (!doc || doc->origin != origin_) return {};
        return {OutboxLoad::ready, doc};
    }
    bool prepare(const study_net::MatchRecord& request) const {
        if (load().state != OutboxLoad::missing) return false;
        return save(OutboxDocument{origin_, request, OutboxStatus::pending, {}});
    }
    bool save(const OutboxDocument& next) const {
        if (!available() || next.origin != origin_ || !valid_outbox(next)) return false;
        const auto current = load();
        if (current.state == OutboxLoad::blocked) return false;
        if (current.state == OutboxLoad::missing) {
            if (next.status != OutboxStatus::pending) return false;
        } else {
            const auto& old = *current.document;
            if (old.request != next.request) return false;
            if (old.status == OutboxStatus::stopped && next.status != OutboxStatus::stopped) return false;
            if (old.status == OutboxStatus::confirmed &&
                (next.status != OutboxStatus::confirmed || old.receipt->row != next.receipt->row)) return false;
        }
        try {
            const auto body = outbox_json(next) + "\n";
            return body.size() <= kJsonBodyBytes && writer_(file_.u8string(), body);
        } catch (const std::exception&) { return false; }
    }
private:
    std::string origin_;
    std::filesystem::path file_;
    AccountFileLock lock_;
    Writer writer_;
};
} // namespace study_meta
