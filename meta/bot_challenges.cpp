#include "bot_challenges.h"
#include "database.h"
#include "protocol.h"
#include "../bot/bot_onnx.h"
#include "../bot/reward_replay.h"
#include "httplib.h"
#include "json_routes.h"
#include <charconv>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace meta {
namespace {
using Clock=std::chrono::steady_clock;
struct Ticket { int64_t player; uint64_t seed; bot::Opponent opponent; Clock::time_point issued; };
struct State {
    std::mutex mutex, verifier;
    std::unordered_map<std::string,Ticket> tickets;
    std::vector<bot::Opponent> opponents=bot::discover_opponents();
};
void reply(httplib::Response& res,int status,const std::string& body) {
    res.status=status;res.set_header("Cache-Control","no-store");
    res.set_header("Access-Control-Allow-Origin","*");res.set_content(body,"application/json");
}
void error(httplib::Response& res,int status,const char* why){reply(res,status,proto::error_json(why));}
bool valid_ticket(const std::string& s) {
    return s.size()==32 && s.find_first_not_of("0123456789abcdef")==std::string::npos;
}
}
void register_bot_challenges(httplib::Server& svr,Database& db,
                            std::function<std::optional<std::string>()> secure_id) {
    auto state=std::make_shared<State>();
    json_post(svr, "/v1/bots/challenge",[state,&db,secure_id](const httplib::Request& req,httplib::Response& res){
        auto player=db.getByToken(proto::find_string(req.body,"token"));
        if(!player){error(res,401,"unknown_token");return;}
        const auto id=proto::find_string(req.body,"opponent_id");
        auto entry=std::find_if(state->opponents.begin(),state->opponents.end(),[&](const bot::Opponent& e){return e.id==id;});
        if(entry==state->opponents.end()){error(res,404,"unknown_opponent");return;}
#ifndef TETRIS_HAS_ONNXRUNTIME
        if(entry->path!="@heuristic"){error(res,503,"model_unavailable");return;}
#endif
        auto ticket=secure_id();
        if(!ticket){error(res,503,"entropy_unavailable");return;}
        uint64_t seed=0;
        std::from_chars(ticket->data(),ticket->data()+15,seed,16);
        const auto now=Clock::now();
        std::lock_guard<std::mutex> lock(state->mutex);
        for(auto it=state->tickets.begin();it!=state->tickets.end();) {
            if(now-it->second.issued>std::chrono::minutes(15) || it->second.player==player->id)it=state->tickets.erase(it);
            else ++it;
        }
        if(state->tickets.size()>=256){error(res,503,"challenge_capacity");return;}
        state->tickets.emplace(*ticket,Ticket{player->id,seed,*entry,now});
        reply(res,200,"{\"ticket\":\""+*ticket+"\",\"seed\":"+std::to_string(seed)+
            ",\"input_ticks\":"+std::to_string(entry->inputIntervalTicks)+
            ",\"think_ticks\":"+std::to_string(entry->thinkTicks)+
            ",\"min_piece_ticks\":"+std::to_string(entry->minPieceTicks)+"}");
    });
    json_post(svr, "/v1/bots/claim",[state,&db](const httplib::Request& req,httplib::Response& res){
        const auto token=proto::find_string(req.body,"token");
        auto player=db.getByToken(token);
        if(!player){error(res,401,"unknown_token");return;}
        const auto id=proto::find_string(req.body,"ticket");
        if(!valid_ticket(id)){error(res,400,"invalid_ticket");return;}
        auto success=[&](int earned){
            auto current=db.getByToken(token);
            if(!current){error(res,401,"unknown_token");return;}
            reply(res,200,"{\"awarded_bp\":"+std::to_string(earned)+",\"bp\":"+std::to_string(current->bp)+"}");
        };
        if(auto prior=db.botReward(player->id,id)){success(*prior);return;}
        Ticket ticket;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            auto it=state->tickets.find(id);
            if(it==state->tickets.end() || it->second.player!=player->id){error(res,403,"unknown_challenge");return;}
            ticket=it->second;
        }
        auto elapsed=Clock::now()-ticket.issued;
        if(elapsed>std::chrono::minutes(15)){error(res,410,"challenge_expired");return;}
        std::vector<uint8_t> inputs;
        if(!bot::decode_inputs(proto::find_string(req.body,"inputs_hex"),inputs)){error(res,400,"invalid_replay");return;}
        if(std::chrono::duration<double>(elapsed).count()+0.25 < inputs.size()/60.0){error(res,409,"replay_too_fast");return;}
        // One replay verification at a time. Never queue expensive jobs behind each other.
        std::unique_lock<std::mutex> worker(state->verifier,std::try_to_lock);
        if(!worker.owns_lock()){res.set_header("Retry-After","2");error(res,429,"verifier_busy");return;}
        bot::BotOnnx model;
        if(ticket.opponent.path!="@heuristic" && !model.Load(ticket.opponent.path)){error(res,503,"model_unavailable");return;}
        auto picker=[&](const SimGame& sim,int& col,int& rot){
            bool ok=ticket.opponent.path=="@heuristic"?bot::heuristic_placement(sim,col,rot):model.Infer(sim,col,rot);
            return ok || bot::fallback_placement(sim,col,rot);
        };
        if(!bot::verify_victory(ticket.seed,ticket.opponent,inputs,picker)) {
            // A failed proof cannot be refined indefinitely against the same challenge.
            std::lock_guard<std::mutex> lock(state->mutex);state->tickets.erase(id);
            error(res,422,"victory_not_verified");return;
        }
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            // A new challenge may have superseded this one while verification ran.
            if(!state->tickets.count(id)){error(res,409,"challenge_superseded");return;}
            auto earned=db.saveBotWin(player->id,id,ticket.opponent.id);
            if(!earned){error(res,503,"reward_save_failed");return;}
            state->tickets.erase(id);
            success(*earned);
        }
    });
}
}
