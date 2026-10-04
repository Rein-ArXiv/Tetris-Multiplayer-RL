// Call Round directly: this executable deliberately does not include Session.
#include "simulation/input_mask.h"
#include "simulation/state_hash.h"
#include <charconv>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

template<class T> T number(const char* raw) {
    const std::string text(raw);
    T value{};
    const auto parsed = std::from_chars(text.data(), text.data()+text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size())
        throw std::invalid_argument("numeric argument");
    return value;
}
void emit(const study_round::Round& round, int step) {
    const auto bytes = study_hash::state_bytes(round);
    if (!bytes.ok()) throw std::runtime_error("state capacity");
    std::cout << std::dec << step << ' ' << round.score() << ' ' << round.finished() << ' ';
    for (std::size_t i=0; i<bytes.size(); ++i)
        std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(bytes.data()[i]);
    std::cout << '\n';
}
int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::invalid_argument("seed interval [masks...]");
        auto round = study_round::Round::create_seeded({}, number<std::uint64_t>(argv[1]), number<int>(argv[2]));
        if (!round) throw std::invalid_argument("round configuration");
        emit(*round, -1);
        for (int i=3; i<argc; ++i) {
            const auto input = study_input::decode(number<unsigned>(argv[i]));
            if (!input) throw std::invalid_argument("input mask");
            const auto result = round->tick(input->horizontal,input->clockwise,input->soft_drop,input->hard_drop);
            if (result == study_round::Step::invalid) throw std::runtime_error("invalid transition");
            emit(*round, static_cast<int>(result));
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
