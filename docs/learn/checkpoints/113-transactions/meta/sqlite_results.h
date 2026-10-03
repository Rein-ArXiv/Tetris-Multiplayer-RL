#pragma once
#include "meta/sqlite_handle.h"
#include "meta/schema.h"
#include "meta/history_queries.h"
#include "meta/migrations.h"
#include "meta/reward_policy.h"
#include <functional>
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

struct RecentMatch {
    std::uint64_t row;
    std::uint64_t key;
};

enum class RewardPoint { after_match, after_a, after_b, before_commit };
using RewardHook = std::function<void(RewardPoint)>;
struct SavedAwards { int a; int b; int policy; };

class SqliteResults {
public:
    explicit SqliteResults(const std::string& path) : db_(path) {
        db_.exec("PRAGMA foreign_keys=ON; PRAGMA synchronous=FULL;");
        {
            Statement enabled(db_.get(), "PRAGMA foreign_keys");
            if (!enabled.next() || enabled.integer_column(0) != 1)
                throw std::runtime_error("foreign keys unavailable");
        }
        migrate_study(db_);
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
                     "ON CONFLICT(player_id,icon_id) DO NOTHING;"
                     "INSERT INTO wallets(player_id) VALUES('101'),('202') ON CONFLICT(player_id) DO NOTHING;");
            db_.exec("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_.get(), "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }
    void add_player(std::uint64_t id, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        ImmediateTransaction transaction(db_);
        Statement q(db_.get(), "INSERT INTO players(id,display_name) VALUES(?1,?2)");
        q.text(1, std::to_string(id));
        q.text(2, name);
        q.done();
        Statement own(db_.get(), "INSERT INTO player_icons(player_id,icon_id) VALUES(?1,'default')");
        own.text(1, std::to_string(id));
        own.done();
        Statement wallet(db_.get(), "INSERT INTO wallets(player_id) VALUES(?1)");
        wallet.text(1, std::to_string(id));
        wallet.done();
        transaction.commit();
    }

    std::optional<std::string> selected_icon(std::uint64_t player) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "SELECT selected_icon_id FROM players WHERE id=?1");
        q.text(1, std::to_string(player));
        if (!q.next()) return std::nullopt;
        return q.text_column(0);
    }
    bool select_icon(std::uint64_t player, const std::string& icon) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "UPDATE players SET selected_icon_id=?1 WHERE id=?2 AND EXISTS "
            "(SELECT 1 FROM player_icons WHERE player_id=?2 AND icon_id=?1)");
        q.text(1, icon);
        q.text(2, std::to_string(player));
        q.done();
        return sqlite3_changes(db_.get()) != 0;
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
    // Both participant positions contribute to one bounded, ordered result.
    std::vector<RecentMatch> recent(std::uint64_t player, unsigned limit) {
        if (player == 0 || limit == 0 || limit > 50)
            throw std::invalid_argument("recent requires player > 0 and limit 1..50");
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), kRecentMatches);
        q.text(1, std::to_string(player));
        q.integer(2, limit);
        std::vector<RecentMatch> result;
        while (q.next()) {
            const auto row = q.integer_column(0);
            if (row <= 0)
                throw std::runtime_error("invalid recent row id");
            result.push_back({static_cast<std::uint64_t>(row), decimal(q.text_column(1))});
        }
        return result;
    }
    std::uint64_t count() {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "SELECT count(*) FROM matches");
        if (!q.next())
            throw std::runtime_error("count missing");
        return static_cast<std::uint64_t>(q.integer_column(0));
    }
    int balance(std::uint64_t player) {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_balance(player);
    }
    std::optional<SavedAwards> awards(std::uint64_t key) {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(),
            "SELECT a.bp_delta,b.bp_delta,a.policy,b.policy FROM matches m "
            "JOIN match_rewards a ON a.match_id=m.id AND a.player_id=m.player_a "
            "JOIN match_rewards b ON b.match_id=m.id AND b.player_id=m.player_b WHERE m.match_key=?1");
        q.text(1,std::to_string(key));
        if(!q.next())return std::nullopt;
        const auto a=q.integer_column(0), b=q.integer_column(1), policy=q.integer_column(2);
        if(a<0 || a>10 || b<0 || b>10 || policy<0 || policy>1 || policy!=q.integer_column(3))
            throw std::runtime_error("invalid saved awards");
        return SavedAwards{static_cast<int>(a),static_cast<int>(b),static_cast<int>(policy)};
    }
    // The hook is only for local fault experiments; HTTP never supplies one.
    SqlResult put(const study_net::MatchRecord& record, const RewardHook& hook={}) {
        study_net::MatchSubmission validator(0);
        if (!validator.prepare(record))
            return {SqlStatus::invalid, {}};
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            ImmediateTransaction transaction(db_);
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
            if(hook)hook(RewardPoint::after_match);
            const auto amounts=awards_for(record.winner);
            credit(row,record.player_a,amounts.a);
            if(hook)hook(RewardPoint::after_a);
            credit(row,record.player_b,amounts.b);
            if(hook)hook(RewardPoint::after_b);
            if(hook)hook(RewardPoint::before_commit);
            transaction.commit();
            return {SqlStatus::accepted,
                    {record.key, static_cast<std::uint64_t>(row), record.player_a, record.player_b}};
        } catch (const std::exception&) {
            // A SQL/storage error is not an empty result or a confirmed receipt.
            return {SqlStatus::storage_error, {}};
        }
    }
private:
    int read_balance(std::uint64_t player) {
        Statement q(db_.get(),"SELECT bp FROM wallets WHERE player_id=?1");
        q.text(1,std::to_string(player));
        if(!q.next())throw std::runtime_error("wallet missing");
        const auto value=q.integer_column(0);
        if(value<0 || value>kBalanceLimit)throw std::runtime_error("invalid wallet value");
        return static_cast<int>(value);
    }
    void credit(sqlite3_int64 match, std::uint64_t player, int amount) {
        // Called only while put owns the connection's write transaction and mutex.
        const int after=credited_balance(read_balance(player),amount);
        Statement wallet(db_.get(),"UPDATE wallets SET bp=?1 WHERE player_id=?2");
        wallet.integer(1,after);
        wallet.text(2,std::to_string(player));
        wallet.done();
        if(sqlite3_changes(db_.get())!=1)throw std::runtime_error("wallet update missing");
        Statement reward(db_.get(),"INSERT INTO match_rewards(match_id,player_id,bp_delta,policy) VALUES(?1,?2,?3,1)");
        reward.integer(1,match);
        reward.text(2,std::to_string(player));
        reward.integer(3,amount);
        reward.done();
    }
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
