#pragma once
#include "meta/sqlite_handle.h"
#include "meta/schema.h"
#include "net/match_submission.h"
#include <charconv>
#include <mutex>
#include <optional>
#include <vector>

namespace study_meta {

enum class SqlStatus { accepted, conflict, invalid, storage_error };

struct SqlResult {
    SqlStatus status = SqlStatus::storage_error;
    study_net::Receipt receipt{};
};

struct StoredMatch {
    std::uint64_t row;
    study_net::MatchRecord record;
};

class SqliteResults {
public:
    explicit SqliteResults(const std::string& path) : db_(path) {
        db_.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
        {
            Statement enabled(db_.get(), "PRAGMA foreign_keys");
            if (!enabled.next() || enabled.integer_column(0) != 1)
                throw std::runtime_error("foreign keys unavailable");
        }
        db_.exec(kStudySchema);
    }
    SqliteResults(const SqliteResults&) = delete;
    SqliteResults& operator=(const SqliteResults&) = delete;
    // Fixture bootstrap only; no account credential or public registration API.
    void seed_demo() {
        std::lock_guard<std::mutex> lock(mutex_);
        db_.exec("BEGIN;");
        try {
            db_.exec("INSERT INTO icons(id,label) VALUES('default','Default'),('ruby','Ruby') ON CONFLICT(id) DO NOTHING;"
                     "INSERT INTO players(id,display_name) VALUES('101','Player A'),('202','Player B') ON CONFLICT(id) DO NOTHING;"
                     "INSERT INTO player_icons(player_id,icon_id) VALUES('101','default'),('202','default') "
                     "ON CONFLICT(player_id,icon_id) DO NOTHING;");
            db_.exec("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_.get(), "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }
    void add_player(std::uint64_t id, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "INSERT INTO players(id,display_name) VALUES(?1,?2)");
        q.text(1, std::to_string(id));
        q.text(2, name);
        q.done();
    }
    bool rename_player(std::uint64_t id, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "UPDATE players SET display_name=?1 WHERE id=?2");
        q.text(1, name);
        q.text(2, std::to_string(id));
        q.done();
        return sqlite3_changes(db_.get()) != 0;
    }
    bool grant_icon(std::uint64_t player, const std::string& icon) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "INSERT INTO player_icons(player_id,icon_id) VALUES(?1,?2) "
                              "ON CONFLICT(player_id,icon_id) DO NOTHING");
        q.text(1, std::to_string(player));
        q.text(2, icon);
        q.done();
        return sqlite3_changes(db_.get()) != 0;
    }
    std::vector<std::string> icons(std::uint64_t player) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(),
                    "SELECT icon_id FROM player_icons WHERE player_id=?1 ORDER BY icon_id");
        q.text(1, std::to_string(player));
        std::vector<std::string> out;
        while (q.next())
            out.push_back(q.text_column(0));
        return out;
    }
    std::optional<StoredMatch> lookup(std::uint64_t key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_match(key);
    }
    std::uint64_t count() {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "SELECT count(*) FROM matches");
        if (!q.next())
            throw std::runtime_error("count missing");
        return static_cast<std::uint64_t>(q.integer_column(0));
    }
    SqlResult put(const study_net::MatchRecord& record) {
        study_net::MatchSubmission validator(0);
        if (!validator.prepare(record))
            return {SqlStatus::invalid, {}};
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            if (const auto found = read_match(record.key)) {
                if (found->record != record)
                    return {SqlStatus::conflict, {}};
                return {SqlStatus::accepted,
                        {record.key, found->row, record.player_a, record.player_b}};
            }
            if (!has_player(record.player_a) || !has_player(record.player_b))
                return {SqlStatus::invalid, {}};
            Statement q(db_.get(), "INSERT INTO matches(match_key,round,player_a,player_b,winner,ticks,score_a,score_b,lines_a,lines_b) "
                                  "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10)");
            q.text(1, std::to_string(record.key));
            q.text(2, std::to_string(record.round));
            q.text(3, std::to_string(record.player_a));
            q.text(4, std::to_string(record.player_b));
            if (record.winner == study_net::MatchRecord::draw)
                q.null(5);
            else
                q.text(5, std::to_string(record.winner == study_net::MatchRecord::a
                                             ? record.player_a
                                             : record.player_b));
            q.text(6, std::to_string(record.ticks));
            q.text(7, std::to_string(record.score_a));
            q.text(8, std::to_string(record.score_b));
            q.integer(9, record.lines_a);
            q.integer(10, record.lines_b);
            q.done();
            const auto row = sqlite3_last_insert_rowid(db_.get());
            if (row <= 0)
                throw std::runtime_error("invalid row identity");
            return {SqlStatus::accepted,
                    {record.key, static_cast<std::uint64_t>(row), record.player_a, record.player_b}};
        } catch (const std::exception&) {
            // A SQL/storage error is not an empty result or a confirmed receipt.
            return {SqlStatus::storage_error, {}};
        }
    }
private:
    static std::uint64_t decimal(const std::string& text) {
        std::uint64_t value = 0;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
        if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
            || std::to_string(value) != text)
            throw std::runtime_error("invalid unsigned decimal");
        return value;
    }
    bool has_player(std::uint64_t id) {
        Statement q(db_.get(), "SELECT 1 FROM players WHERE id=?1");
        q.text(1, std::to_string(id));
        return q.next();
    }
    std::optional<StoredMatch> read_match(std::uint64_t key) {
        Statement q(db_.get(), "SELECT id,round,player_a,player_b,winner,ticks,score_a,score_b,lines_a,lines_b "
                              "FROM matches WHERE match_key=?1");
        q.text(1, std::to_string(key));
        if (!q.next())
            return std::nullopt;
        const auto row = q.integer_column(0);
        if (row <= 0)
            throw std::runtime_error("invalid row id");
        study_net::MatchRecord r;
        r.key = key;
        r.round = decimal(q.text_column(1));
        r.player_a = decimal(q.text_column(2));
        r.player_b = decimal(q.text_column(3));
        if (q.is_null(4))
            r.winner = study_net::MatchRecord::draw;
        else {
            const auto winner = decimal(q.text_column(4));
            if (winner != r.player_a && winner != r.player_b)
                throw std::runtime_error("invalid winner relation");
            r.winner = winner == r.player_a ? study_net::MatchRecord::a : study_net::MatchRecord::b;
        }
        r.ticks = decimal(q.text_column(5));
        r.score_a = decimal(q.text_column(6));
        r.score_b = decimal(q.text_column(7));
        const auto la = q.integer_column(8), lb = q.integer_column(9);
        if (la < 0 || lb < 0 || la > UINT32_MAX || lb > UINT32_MAX)
            throw std::runtime_error("invalid line range");
        r.lines_a = static_cast<std::uint32_t>(la);
        r.lines_b = static_cast<std::uint32_t>(lb);
        study_net::MatchSubmission validator(0);
        if (!validator.prepare(r))
            throw std::runtime_error("invalid saved record");
        return StoredMatch{static_cast<std::uint64_t>(row), r};
    }
    SqliteDb db_;
    std::mutex mutex_;
};
} // namespace study_meta
