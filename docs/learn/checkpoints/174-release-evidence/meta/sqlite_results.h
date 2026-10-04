#pragma once
#include "meta/sqlite_handle.h"
#include "meta/account_change.h"
#include "meta/bot_settlement.h"
#include "meta/schema.h"
#include "meta/history_queries.h"
#include "meta/migrations.h"
#include "meta/reward_policy.h"
#include "meta/progression_policy.h"
#include "meta/profile_wire.h"
#include "meta/ranking_wire.h"
#include "meta/shop_catalog.h"
#include "meta/shop_view.h"
#include <algorithm>
#include <functional>
#include "net/match_submission.h"
#include <charconv>
#include <mutex>
#include <optional>
#include <vector>

namespace study_meta {

enum class SqlStatus { accepted, conflict, invalid, storage_error };

struct SavedAwards { int a=0; int b=0; int policy=0; };

struct SqlResult {
    SqlStatus status = SqlStatus::storage_error;
    study_net::Receipt receipt{};
    SavedAwards awards{};
};

struct StoredMatch {
    std::uint64_t row;
    study_net::MatchRecord record;
};

struct RecentMatch {
    std::uint64_t row;
    std::uint64_t key;
};

enum class RewardPoint { after_match, after_a, after_b, after_progress_a, after_progress_b, before_commit };
using RewardHook = std::function<void(RewardPoint)>;

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
                     "INSERT INTO wallets(player_id) VALUES('101'),('202') ON CONFLICT(player_id) DO NOTHING;"
                     "INSERT INTO careers(player_id) VALUES('101'),('202') ON CONFLICT(player_id) DO NOTHING;");
            db_.exec("COMMIT;");
        } catch (...) {
            sqlite3_exec(db_.get(), "ROLLBACK;", nullptr, nullptr, nullptr);
            throw;
        }
    }
    // Canonical decimal TEXT IDs span uint64, beyond SQLite signed INTEGER.
    // Length then byte order is numeric order for positive canonical decimals.
    Ranking ranking() {
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(),"SELECT player_id,rp FROM careers "
            "ORDER BY rp DESC,length(player_id) ASC,player_id COLLATE BINARY ASC LIMIT ?1");
        q.integer(1,static_cast<sqlite3_int64>(kRankingLimit));
        Ranking rows;
        while(q.next()) {
            const auto text=q.text_column(0);
            const auto id=decimal(text);
            const auto rp=q.integer_column(1);
            if(!id || std::to_string(id)!=text || rp<0 || rp>kProgressLimit)
                throw std::runtime_error("invalid ranking row");
            rows.push_back({id,static_cast<int>(rp)});
        }
        if(!valid_ranking(rows))throw std::runtime_error("invalid ranking order");
        return rows; // only SQLITE_DONE publishes the complete list
    }
    void add_player(std::uint64_t id, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        ImmediateTransaction transaction(db_);
        insert_player(id,name);
        transaction.commit();
    }
    enum class Registration { created, collision };
    Registration register_account(std::uint64_t id, const std::string& digest) {
        if(id==0 || digest.size()!=64 || digest.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("invalid registration identity");
        std::lock_guard<std::mutex> lock(mutex_);
        ImmediateTransaction transaction(db_);
        if(has_player(id))return Registration::collision;
        Statement exists(db_.get(),"SELECT 1 FROM account_keys WHERE token_hash=?1");
        exists.text(1,digest);
        if(exists.next())return Registration::collision;
        insert_player(id,"Guest");
        Statement key(db_.get(),"INSERT INTO account_keys(player_id,token_hash) VALUES(?1,?2)");
        key.text(1,std::to_string(id));key.text(2,digest);key.done();
        transaction.commit();
        return Registration::created;
    }
    std::optional<PublicProfile> profile_by_hash(const std::string& digest) {
        if(digest.size()!=64 || digest.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("invalid credential hash");
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(),"SELECT a.player_id,c.rp,c.xp,w.bp FROM account_keys a "
            "LEFT JOIN careers c ON c.player_id=a.player_id "
            "LEFT JOIN wallets w ON w.player_id=a.player_id WHERE a.token_hash=?1");
        q.text(1,digest);
        if(!q.next())return std::nullopt;
        const auto id=decimal(q.text_column(0));
        const auto rp=q.integer_column(1),xp=q.integer_column(2),bp=q.integer_column(3);
        if(id==0 || rp<0 || rp>kProgressLimit || xp<0 || xp>kProgressLimit || bp<0 || bp>kBalanceLimit)
            throw std::runtime_error("invalid account profile");
        return PublicProfile{id,static_cast<int>(rp),static_cast<int>(xp),static_cast<int>(bp)};
    }

    ChangeResult change_account(const ChangeRequest& request, const ChangeHook& hook={}) {
        std::lock_guard<std::mutex> lock(mutex_);
        return change_credentials(db_, request, hook);
    }

    bool credential_is_current(std::uint64_t id, std::uint64_t epoch) {
        if (id == 0 || epoch > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        Statement q(db_.get(), "SELECT 1 FROM account_keys WHERE player_id=?1 AND auth_epoch=?2");
        q.text(1, std::to_string(id)); q.integer(2, static_cast<std::int64_t>(epoch));
        return q.next();
    }

    // Catalog bootstrap is trusted service setup, not a public grant endpoint.
    void seed_shop() {
        std::lock_guard<std::mutex> lock(mutex_);
        ImmediateTransaction transaction(db_);
        for(const auto& icon:kShopIcons) {
            Statement q(db_.get(),"INSERT INTO icons(id,label) VALUES(?1,?2) ON CONFLICT(id) DO NOTHING");
            q.text(1,icon.id);q.text(2,icon.label);q.done();
        }
        transaction.commit();
    }
    ShopResult inventory(const std::string& digest) {
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            const auto view=read_shop(digest);
            return view ? ShopResult{ShopStatus::ok,view} : ShopResult{ShopStatus::unknown_account,{}};
        }catch(const std::exception&){return {ShopStatus::storage_error,{}};}
    }
    ShopResult buy_icon(const std::string& digest,const std::string& icon_id) {
        const auto* icon=shop_icon(icon_id);
        if(!icon)return {ShopStatus::unknown_icon,{}};
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            ImmediateTransaction transaction(db_);
            const auto before=read_shop(digest);
            if(!before)return {ShopStatus::unknown_account,{}};
            if(owns(*before,icon_id))return {ShopStatus::already_owned,{}};
            if(before->profile.bp<icon->price)return {ShopStatus::insufficient_bp,{}};
            Statement debit(db_.get(),"UPDATE wallets SET bp=bp-?1 WHERE player_id=?2 "
                "AND typeof(bp)='integer' AND bp>=?1");
            debit.integer(1,icon->price);debit.text(2,std::to_string(before->profile.player_id));debit.done();
            if(sqlite3_changes(db_.get())!=1)throw std::runtime_error("debit missing");
            Statement grant(db_.get(),"INSERT INTO player_icons(player_id,icon_id) VALUES(?1,?2)");
            grant.text(1,std::to_string(before->profile.player_id));grant.text(2,icon_id);grant.done();
            if(sqlite3_changes(db_.get())!=1)throw std::runtime_error("grant missing");
            const auto after=read_shop(digest);
            if(!after)throw std::runtime_error("purchased profile missing");
            transaction.commit();
            return {ShopStatus::ok,after};
        }catch(const std::exception&){return {ShopStatus::storage_error,{}};}
    }
    ShopResult choose_icon(const std::string& digest,const std::string& icon_id) {
        if(!shop_icon(icon_id))return {ShopStatus::unknown_icon,{}};
        std::lock_guard<std::mutex> lock(mutex_);
        try {
            ImmediateTransaction transaction(db_);
            const auto before=read_shop(digest);
            if(!before)return {ShopStatus::unknown_account,{}};
            if(!owns(*before,icon_id))return {ShopStatus::not_owned,{}};
            Statement q(db_.get(),"UPDATE players SET selected_icon_id=?1 WHERE id=?2 AND EXISTS "
                "(SELECT 1 FROM player_icons WHERE player_id=?2 AND icon_id=?1)");
            q.text(1,icon_id);q.text(2,std::to_string(before->profile.player_id));q.done();
            if(sqlite3_changes(db_.get())!=1)throw std::runtime_error("selection missing");
            const auto after=read_shop(digest);
            if(!after)throw std::runtime_error("selected profile missing");
            transaction.commit();
            return {ShopStatus::ok,after};
        }catch(const std::exception&){return {ShopStatus::storage_error,{}};}
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
    std::optional<BotReceipt> bot_receipt(const std::string& ticket) {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_bot_receipt(db_,ticket);
    }
    BotReceipt award_bot(const std::string& ticket,std::uint64_t player,
                         const std::string& opponent,std::uint32_t revision,
                         std::int64_t day,BotRewardPolicy policy,const BotRewardHook& hook={}) {
        std::lock_guard<std::mutex> lock(mutex_);
        return settle_bot(db_,ticket,player,opponent,revision,day,policy,hook);
    }
    std::optional<SavedAwards> awards(std::uint64_t key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_awards(key);
    }
    Career career(std::uint64_t player) {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_career(player);
    }
    std::optional<ProgressPair> progress(std::uint64_t key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return read_progress(key);
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
                const auto saved=read_awards(record.key);
                if(!saved || !read_progress(record.key))throw std::runtime_error("saved receipt missing");
                return {SqlStatus::accepted,
                        {record.key, found->row, record.player_a, record.player_b}, *saved};
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
            const auto a=read_career(record.player_a), b=read_career(record.player_b);
            const auto progress=progress_for(a.rp,b.rp,record.winner);
            advance(row,record.player_a,a,progress.a);
            if(hook)hook(RewardPoint::after_progress_a);
            advance(row,record.player_b,b,progress.b);
            if(hook)hook(RewardPoint::after_progress_b);
            if(hook)hook(RewardPoint::before_commit);
            transaction.commit();
            return {SqlStatus::accepted,
                    {record.key, static_cast<std::uint64_t>(row), record.player_a, record.player_b},
                    {amounts.a,amounts.b,1}};
        } catch (const std::exception&) {
            // A SQL/storage error is not an empty result or a confirmed receipt.
            return {SqlStatus::storage_error, {}};
        }
    }
private:
    static bool owns(const ShopView& view,const std::string& icon) {
        return std::find(view.owned.begin(),view.owned.end(),icon)!=view.owned.end();
    }
    // One statement gives inventory readers a consistent SQLite snapshot.
    std::optional<ShopView> read_shop(const std::string& digest) {
        if(digest.size()!=64 || digest.find_first_not_of("0123456789abcdef")!=std::string::npos)
            throw std::invalid_argument("invalid credential hash");
        Statement q(db_.get(),"SELECT a.player_id,c.rp,c.xp,w.bp,p.selected_icon_id,i.icon_id FROM account_keys a "
            "LEFT JOIN players p ON p.id=a.player_id LEFT JOIN careers c ON c.player_id=a.player_id "
            "LEFT JOIN wallets w ON w.player_id=a.player_id LEFT JOIN player_icons i ON i.player_id=a.player_id "
            "WHERE a.token_hash=?1 ORDER BY i.icon_id");
        q.text(1,digest);
        if(!q.next())return std::nullopt;
        const auto id=decimal(q.text_column(0));
        const auto rp=q.integer_column(1),xp=q.integer_column(2),bp=q.integer_column(3);
        if(id==0 || rp<0 || rp>kProgressLimit || xp<0 || xp>kProgressLimit || bp<0 || bp>kBalanceLimit)
            throw std::runtime_error("invalid inventory profile");
        ShopView result{{id,static_cast<int>(rp),static_cast<int>(xp),static_cast<int>(bp)},q.text_column(4),{}};
        do {result.owned.push_back(q.text_column(5));}while(q.next());
        if(!owns(result,result.selected))throw std::runtime_error("selected icon is not owned");
        return result;
    }
    // Called with the mutex and a write transaction already held.
    void insert_player(std::uint64_t id, const std::string& name) {
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
        Statement career(db_.get(), "INSERT INTO careers(player_id) VALUES(?1)");
        career.text(1, std::to_string(id));
        career.done();
    }
    Career read_career(std::uint64_t player) {
        Statement q(db_.get(),"SELECT rp,xp FROM careers WHERE player_id=?1");
        q.text(1,std::to_string(player));
        if(!q.next())throw std::runtime_error("career missing");
        const auto rp=q.integer_column(0), xp=q.integer_column(1);
        if(rp<0 || rp>kProgressLimit || xp<0 || xp>kProgressLimit)
            throw std::runtime_error("invalid career");
        return {static_cast<int>(rp),static_cast<int>(xp)};
    }
    void advance(sqlite3_int64 match, std::uint64_t player, Career before, ProgressAward award) {
        // put owns the write transaction and mutex throughout both participants.
        const int xp=add_xp(before.xp,award.xp);
        Statement q(db_.get(),"UPDATE careers SET rp=?1,xp=?2 WHERE player_id=?3");
        q.integer(1,award.after);q.integer(2,xp);q.text(3,std::to_string(player));q.done();
        if(sqlite3_changes(db_.get())!=1)throw std::runtime_error("career update missing");
        Statement receipt(db_.get(),"INSERT INTO match_progress VALUES(?1,?2,?3,?4,?5,?6)");
        receipt.integer(1,match);receipt.text(2,std::to_string(player));
        receipt.integer(3,award.before);receipt.integer(4,award.after);
        receipt.integer(5,award.xp);receipt.integer(6,award.policy);receipt.done();
    }
    std::optional<ProgressPair> read_progress(std::uint64_t key) {
        Statement q(db_.get(),"SELECT a.rp_before,a.rp_after,a.xp_delta,a.policy,"
            "b.rp_before,b.rp_after,b.xp_delta,b.policy FROM matches m "
            "JOIN match_progress a ON a.match_id=m.id AND a.player_id=m.player_a "
            "JOIN match_progress b ON b.match_id=m.id AND b.player_id=m.player_b WHERE m.match_key=?1");
        q.text(1,std::to_string(key));
        if(!q.next())return std::nullopt;
        int values[8]{};
        for(int i=0;i<8;++i) {
            const auto value=q.integer_column(i);
            if(value<0 || value>kProgressLimit)throw std::runtime_error("invalid progress receipt");
            values[i]=static_cast<int>(value);
        }
        ProgressPair out{{values[0],values[1],values[2],values[3]},
                         {values[4],values[5],values[6],values[7]}};
        if(out.a.policy!=out.b.policy || out.a.policy>1)
            throw std::runtime_error("invalid progress policy");
        if(out.a.policy==0) {
            for(int value:values)if(value!=0)throw std::runtime_error("invalid legacy progress");
        }else if(!((out.a.xp==100 && out.b.xp==50) || (out.a.xp==50 && out.b.xp==100) ||
                  (out.a.xp==0 && out.b.xp==0 && out.a.before==out.a.after && out.b.before==out.b.after))) {
            throw std::runtime_error("invalid progress awards");
        }
        return out;
    }
    std::optional<SavedAwards> read_awards(std::uint64_t key) {
        Statement q(db_.get(),
            "SELECT a.bp_delta,b.bp_delta,a.policy,b.policy FROM matches m "
            "JOIN match_rewards a ON a.match_id=m.id AND a.player_id=m.player_a "
            "JOIN match_rewards b ON b.match_id=m.id AND b.player_id=m.player_b WHERE m.match_key=?1");
        q.text(1,std::to_string(key));
        if(!q.next())return std::nullopt;
        const auto a=q.integer_column(0), b=q.integer_column(1), policy=q.integer_column(2);
        if(a<0 || a>10 || b<0 || b>10 || policy<0 || policy>1 || policy!=q.integer_column(3))
            throw std::runtime_error("invalid saved awards");
        const bool pair_ok=policy==0 ? a==0 && b==0
            : (a==0 && b==0) || (a==10 && b==3) || (a==3 && b==10);
        if(!pair_ok)throw std::runtime_error("invalid saved award pair");
        return SavedAwards{static_cast<int>(a),static_cast<int>(b),static_cast<int>(policy)};
    }
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
