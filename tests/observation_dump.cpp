// Test driver for actual bot::observe, not a second observation implementation.
#include "bot/placement.h"
#include "src/sim_game.h"
#include <array>
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

template<class T> T parse(std::string_view text) {
    T value{};
    const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size())
        throw std::invalid_argument("number");
    return value;
}
template<std::size_t N> void array(const std::array<float,N>& values) {
    std::cout << '[';
    for(std::size_t i=0;i<N;++i) {if(i)std::cout << ',';std::cout << values[i];}
    std::cout << ']';
}
void emit(const SimGame& game) {
    std::array<float,bot::kBoardRows*bot::kBoardCols> board{};
    std::array<float,bot::kNumPieceTypes> current{},next{};
    bot::observe(game,board.data(),current.data(),next.data());
    std::cout << "{\"rows\":" << bot::kBoardRows << ",\"cols\":" << bot::kBoardCols
              << ",\"kinds\":" << bot::kNumPieceTypes << ",\"hash\":" << game.StateHash()
              << ",\"current_id\":" << game.CurrentBlockId() << ",\"next_id\":" << game.NextBlockId()
              << ",\"ended\":" << game.IsGameOver() << ",\"board\":";
    array(board);std::cout << ",\"current\":";array(current);
    std::cout << ",\"next\":";array(next);std::cout << "}\n";
}
int main(int argc,char** argv) {
    try {
        if(argc<2)throw std::invalid_argument("seed [mask|gRows...]");
        SimGame game(parse<std::uint64_t>(argv[1]));emit(game);
        for(int i=2;i<argc;++i) {
            const std::string_view command(argv[i]);
            if(command.size()>1 && command.front()=='g')game.AddPendingGarbage(parse<int>(command.substr(1)));
            else {
                const auto mask=parse<unsigned>(command);
                if(mask>255)throw std::invalid_argument("mask range");
                game.SubmitInput(static_cast<std::uint8_t>(mask));game.Tick();
            }
            emit(game);
        }
    } catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
