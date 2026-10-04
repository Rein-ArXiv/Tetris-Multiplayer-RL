#pragma once
#include "meta/transaction.h"
#include "meta/account_crypto.h"
#include "meta/bot_reward_policy.h"
#include "meta/reward_policy.h"
#include <functional>
#include <optional>
namespace study_meta {
struct BotReceipt { std::uint64_t player;std::string opponent;std::uint32_t revision;int awarded; };
inline std::optional<BotReceipt> read_bot_receipt(SqliteDb& db,const std::string& ticket) {
    if(!credential_hex(ticket,32))throw std::invalid_argument("invalid ticket");
    Statement q(db.get(),"SELECT player_id,opponent,revision,awarded FROM bot_rewards WHERE ticket=?1");
    q.text(1,ticket);if(!q.next())return {};
    const auto id=q.text_column(0);std::size_t end=0;
    const auto player=std::stoull(id,&end);
    const auto revision=q.integer_column(2),award=q.integer_column(3);
    if(!player || end!=id.size() || std::to_string(player)!=id || revision<=0 || revision>UINT32_MAX ||
       award<0 || award>kBalanceLimit)throw std::runtime_error("invalid bot receipt");
    return BotReceipt{player,q.text_column(1),static_cast<std::uint32_t>(revision),static_cast<int>(award)};
}
enum class BotRewardPoint { after_receipt,after_wallet,before_commit };
using BotRewardHook=std::function<void(BotRewardPoint)>;
// Trusted server port, called after verification. Owner holds the connection mutex.
// UTC day and policy come from the server, never from the claim body.
inline BotReceipt settle_bot(SqliteDb& db,const std::string& ticket,std::uint64_t player,
                             const std::string& opponent,std::uint32_t revision,
                             std::int64_t day,BotRewardPolicy policy,const BotRewardHook& hook={}) {
    if(!credential_hex(ticket,32) || !player || opponent.empty() || !revision || day<0)
        throw std::invalid_argument("invalid bot settlement");
    ImmediateTransaction tx(db);
    if(auto prior=read_bot_receipt(db,ticket)) {
        if(prior->player!=player || prior->opponent!=opponent || prior->revision!=revision)
            throw std::invalid_argument("ticket settlement conflict");
        return *prior; // Read-only transaction rolls back; no second wallet credit.
    }
    Statement sum(db.get(),"SELECT COALESCE(SUM(awarded),0) FROM bot_rewards WHERE player_id=?1 AND day=?2");
    sum.text(1,std::to_string(player));sum.integer(2,day);
    if(!sum.next())throw std::runtime_error("daily total missing");
    const int amount=bot_award(policy,sum.integer_column(0));
    Statement receipt(db.get(),"INSERT INTO bot_rewards VALUES(?1,?2,?3,?4,?5,?6)");
    receipt.text(1,ticket);receipt.text(2,std::to_string(player));receipt.text(3,opponent);
    receipt.integer(4,revision);receipt.integer(5,day);receipt.integer(6,amount);receipt.done();
    if(hook)hook(BotRewardPoint::after_receipt);
    Statement credit(db.get(),"UPDATE wallets SET bp=bp+?1 WHERE player_id=?2 "
        "AND typeof(bp)='integer' AND bp>=0 AND bp<=?3");
    credit.integer(1,amount);credit.text(2,std::to_string(player));credit.integer(3,kBalanceLimit-amount);credit.done();
    if(sqlite3_changes(db.get())!=1)throw std::runtime_error("wallet missing or overflowing");
    if(hook)hook(BotRewardPoint::after_wallet);
    if(hook)hook(BotRewardPoint::before_commit);
    tx.commit();return {player,opponent,revision,amount};
}
} // namespace study_meta
