#pragma once
#include "meta/journal_wire.h"
#include "meta/local_account_store.h"
namespace study_meta {
template<std::size_t Bytes>
std::optional<std::string> random_hex() {
    static_assert(Bytes>0 && Bytes<=static_cast<std::size_t>(std::numeric_limits<int>::max()));
    std::array<unsigned char,Bytes> bytes{};
    if (RAND_bytes(bytes.data(),static_cast<int>(bytes.size()))!=1) return {};
    return study_credentials::hex_encode(bytes.data(),bytes.size());
}

// Application-owned folder, one cooperative lock shared with bootstrap.
class JournalStore {
public:
    JournalStore(std::filesystem::path folder, std::string origin,
                 PrivateWriter writer=study_files::write_private_file)
        : folder_(std::move(folder)), origin_(std::move(origin)),
          lock_((folder_/"account.lock").u8string()), writer_(writer) {}
    bool available() const { return lock_.locked() && !origin_.empty() && writer_; }
    const std::string& origin() const { return origin_; }
    bool owns_files() const {
        if (!available()) return false;
        for (const char* name : {"account.json","recovery.json","change.pending.json"}) {
            const auto d=read_local_document(folder_/name);
            if (d.state==LocalRead::missing) continue;
            if (d.state!=LocalRead::ready) return false;
            if (std::string(name)=="change.pending.json") {
                if (!idle_journal(d.body,origin_) && !parse_pending(d.body,origin_)) return false;
            } else {
                const auto j=object(d.body,3);
                if (!j || text(*j,"origin")!=std::optional<std::string>{origin_}) return false;
                const auto id=number(*j,"player_id");
                const auto value=text(*j,std::string(name)=="account.json" ? "token" : "recovery_code");
                if (!id || !*id || !value) return false;
                if (std::string(name)=="account.json" ? !study_credentials::valid_account(*value) : !valid_recovery(*value)) return false;
            }
        }
        return true;
    }
    LoadedChange load_pending() const {
        if (!owns_files()) return {};
        const auto d=read_local_document(folder_/"change.pending.json");
        if (d.state==LocalRead::missing || (d.state==LocalRead::ready && idle_journal(d.body,origin_)))
            return {JournalRead::missing,{}};
        if (d.state!=LocalRead::ready) return {};
        const auto p=parse_pending(d.body,origin_);
        return p ? LoadedChange{JournalRead::ready,*p} : LoadedChange{};
    }
    std::optional<PendingChange> prepare(const std::string& operation) const {
        if (load_pending().state!=JournalRead::missing) return {};
        const bool recovering=operation=="recover";
        if (!recovering && operation!="backup" && operation!="rotate") return {};
        const auto d=read_local_document(folder_/(recovering ? "recovery.json" : "account.json"));
        if (d.state!=LocalRead::ready) return {};
        const auto j=object(d.body,3);
        if (!j) return {};
        const auto id=number(*j,"player_id");
        const auto proof=text(*j,recovering ? "recovery_code" : "token");
        if (!id || !*id || !proof) return {};
        const auto next=operation=="backup" ? std::optional<std::string>{*proof}
            : random_hex<study_credentials::account_bytes>();
        const auto backup=random_hex<recovery_bytes>();
        if (!next || !backup) return {};
        PendingChange p{origin_,operation,*proof,*next,std::string(recovery_prefix)+*backup,*id};
        return valid_pending(p) ? std::optional<PendingChange>{p} : std::nullopt;
    }
    bool begin(const PendingChange& p) const {
        if (p.origin!=origin_ || !valid_pending(p) || load_pending().state!=JournalRead::missing) return false;
        return write("change.pending.json",pending_json(p));
    }
    bool save_recovery(const PendingChange& p,std::uint64_t id) const {
        return same_pending(p,id) && write("recovery.json",Json{{"origin",origin_},
            {"player_id",id},{"recovery_code",p.next_recovery}}.dump()+"\n");
    }
    bool save_access(const PendingChange& p,std::uint64_t id) const {
        return same_pending(p,id) && write("account.json",Json{{"origin",origin_},
            {"player_id",id},{"token",p.next_token}}.dump()+"\n");
    }
    bool remove_pending() const {
        if (load_pending().state!=JournalRead::ready) return false;
        // Clear with the same durable replacement primitive. A small origin-bound
        // idle marker avoids a separate platform-specific durable-unlink contract.
        return write("change.pending.json",Json{{"origin",origin_},{"state","idle"}}.dump()+"\n");
    }
private:
    bool same_pending(const PendingChange& p,std::uint64_t id) const {
        const auto saved=load_pending();
        return id==p.expected_player_id && saved.state==JournalRead::ready &&
            pending_json(saved.request)==pending_json(p);
    }
    bool write(const char* name,const std::string& body) const {
        return available() && body.size()<=kJsonBodyBytes && writer_((folder_/name).u8string(),body);
    }
    std::filesystem::path folder_;
    std::string origin_;
    AccountFileLock lock_;
    PrivateWriter writer_;
};
} // namespace study_meta
