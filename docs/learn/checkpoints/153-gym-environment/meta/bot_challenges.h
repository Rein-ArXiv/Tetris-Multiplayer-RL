#pragma once
#include "meta/sqlite_results.h"
#include "simulation/bot_replay.h"
#include "net/tick_allowance.h"
#include <map>
#include <mutex>
namespace study_meta {
struct BotChallenge {
    std::string ticket;std::uint64_t player,seed;study_bot::Opponent opponent;
    std::int64_t issued_ns;
};
enum class BotClaimStatus { confirmed,unknown,expired,malformed,too_fast,busy,not_verified,superseded,unconfirmed };
struct BotClaim { BotClaimStatus status;int awarded=0; };
// One in-process registry and verifier gate. A service replica needs shared admission.
// Clock values, catalog, policy and actor come from trusted server adapters.
class BotChallenges {
public:
    BotChallenges(SqliteResults& store,std::size_t capacity,std::uint64_t lifetime_ns,
                  study_net::PacePolicy pace,BotRewardPolicy reward)
        :store_(store),capacity_(capacity),lifetime_(lifetime_ns),pace_(pace),reward_(reward) {
        if(!capacity || !lifetime_ns)throw std::invalid_argument("invalid challenge limits");
        (void)bot_award(reward,0);
    }
    bool issue(BotChallenge next) {
        if(!credential_hex(next.ticket,32) || !next.player || !study_bot::valid(next.opponent))return false;
        std::lock_guard<std::mutex> lock(mutex_);
        // Check collision before replacing this player's live challenge.
        if(tickets_.count(next.ticket) || store_.bot_receipt(next.ticket))return false;
        for(auto it=tickets_.begin();it!=tickets_.end();) {
            if(expired(it->second,next.issued_ns))it=tickets_.erase(it);else ++it;
        }
        auto own=std::find_if(tickets_.begin(),tickets_.end(),[&](const auto& row){return row.second.player==next.player;});
        if(own==tickets_.end() && tickets_.size()>=capacity_)return false;
        // Allocate first. An allocation failure preserves the player's old challenge.
        tickets_.emplace(next.ticket,next);
        if(own!=tickets_.end())tickets_.erase(own);
        return true;
    }
    template<class Verify>
    BotClaim claim(std::uint64_t actor,const std::string& key,const std::string& hex,
                   std::int64_t now_ns,std::int64_t utc_day,Verify&& verify) {
        if(!actor || !credential_hex(key,32) || utc_day<0)return {BotClaimStatus::malformed};
        try {
            if(auto saved=store_.bot_receipt(key))
                return saved->player==actor?BotClaim{BotClaimStatus::confirmed,saved->awarded}
                                           :BotClaim{BotClaimStatus::unknown};
            BotChallenge selected;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto it=tickets_.find(key);
                if(it==tickets_.end() || it->second.player!=actor)return {BotClaimStatus::unknown};
                if(expired(it->second,now_ns)){tickets_.erase(it);return {BotClaimStatus::expired};}
                if(now_ns<it->second.issued_ns)return {BotClaimStatus::too_fast};
                selected=it->second;
            }
            std::vector<std::uint8_t> inputs;
            if(!study_bot::decode(hex,pace_.policy().max_ticks,inputs))return {BotClaimStatus::malformed};
            const auto elapsed=static_cast<std::uint64_t>(now_ns)-static_cast<std::uint64_t>(selected.issued_ns);
            if(inputs.size()>pace_.allowed(elapsed))return {BotClaimStatus::too_fast};
            std::unique_lock<std::mutex> worker(verifier_,std::try_to_lock);
            if(!worker.owns_lock())return {BotClaimStatus::busy};
            // Verify is a trusted local implementation, not a request-provided flag.
            const bool victory=verify(selected.seed,selected.opponent,inputs,pace_.policy().max_ticks);
            std::lock_guard<std::mutex> lock(mutex_);
            auto it=tickets_.find(key);
            if(it==tickets_.end())return {BotClaimStatus::superseded};
            if(!victory){tickets_.erase(it);return {BotClaimStatus::not_verified};}
            const auto saved=store_.award_bot(key,actor,selected.opponent.id,selected.opponent.revision,utc_day,reward_);
            tickets_.erase(it);return {BotClaimStatus::confirmed,saved.awarded};
        }catch(const std::exception&) {
            // Storage or local verifier error leaves the live proof retryable.
            return {BotClaimStatus::unconfirmed};
        }
    }
    BotClaim claim(std::uint64_t actor,const std::string& key,const std::string& hex,
                   std::int64_t now_ns,std::int64_t utc_day) {
        return claim(actor,key,hex,now_ns,utc_day,study_bot::verify);
    }
private:
    bool expired(const BotChallenge& ticket,std::int64_t now)const noexcept {
        return now>=ticket.issued_ns &&
            static_cast<std::uint64_t>(now)-static_cast<std::uint64_t>(ticket.issued_ns)>=lifetime_;
    }
    SqliteResults& store_;
    const std::size_t capacity_;const std::uint64_t lifetime_;
    const study_net::TickAllowance pace_;const BotRewardPolicy reward_;
    std::mutex mutex_,verifier_;
    std::map<std::string,BotChallenge> tickets_;
};
} // namespace study_meta
