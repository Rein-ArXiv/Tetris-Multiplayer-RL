// Observe the actual C++ fallback evaluator's choices without duplicating it.
#include "bot/placement.h"
#include "src/sim_game.h"
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

template<class T> T parse(std::string_view text) {
    T value{};
    auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size())
        throw std::invalid_argument("integer required");
    return value;
}
int main(int argc,char** argv) {
    try {
        if(argc!=3)throw std::invalid_argument("seed decisions");
        const int count=parse<int>(argv[2]);
        if(count<0 || count>1000)throw std::invalid_argument("test budget");
        SimGame game(parse<std::uint64_t>(argv[1]));
        for(int i=0;i<count;++i) {
            const auto before=game.StateHash();
            int col=-1,rot=-1;
            bool ok=bot::heuristic_placement(game,col,rot);
            if(game.StateHash()!=before)throw std::runtime_error("search mutated original");
            int lines=ok?game.ApplyPlacement(col,rot):-1;
            std::cout<<"{\"hash\":"<<before<<",\"action\":"
                     <<(ok?bot::encode_action(col,rot):-1)<<",\"lines\":"<<lines
                     <<",\"after\":"<<game.StateHash()<<"}\n";
            if(!ok || game.IsGameOver())break;
        }
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
