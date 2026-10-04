#include "client/opponent_cards.h"
#include "bot/practice_picker.h"
#include "bot/policy_match.h"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

static int run(int argc, char** argv) {
    try {
        if (argc != 4) throw std::invalid_argument("catalog-path stable-id tick-budget");
        std::ifstream input(std::filesystem::u8path(argv[1]));
        if (!input) throw std::runtime_error("cannot open catalog");
        const auto catalog = study_characters::Catalog::read(input);
        // These owned cards are a presentation boundary, not texture handles.
        const auto cards = study_opponent_menu::cards(catalog);
        for (const auto& card : cards)
            std::cout << "card " << std::quoted(card.id) << ' ' << std::quoted(card.name) << ' '
                      << std::quoted(card.icon) << ' ' << std::quoted(card.portrait) << '\n';
        const auto chosen = catalog.select(argv[2]);
        const std::string raw(argv[3]); int budget = 0;
        const auto parsed = std::from_chars(raw.data(), raw.data()+raw.size(), budget);
        if (parsed.ec != std::errc{} || parsed.ptr != raw.data()+raw.size() || budget <= 0)
            throw std::invalid_argument("positive tick budget required");
        if (chosen.behavior.model != "@heuristic")
            throw std::invalid_argument("this practice host supports only its local heuristic");
        study_match::Match match(77, chosen, bot::Mode::practice);
        for (int tick = 0; tick < budget && !match.finished(); ++tick) {
            // Synthetic human input keeps the demonstration independent of window events.
            const auto input = tick % 17 == 16 ? study_input::drop : 0;
            const auto frame = match.tick(input, study_practice::pick, false);
            if (frame.bot.decision_attempted || frame.bot.mask)
                std::cout << "tick " << tick << " decision " << frame.bot.decision_attempted
                          << " mask " << unsigned(frame.bot.mask) << '\n';
        }
        std::cout << "selected " << std::quoted(chosen.id) << " reward_eligible "
                  << match.status().reward_eligible() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}

#ifdef _WIN32
#include "platform/utf8_arguments.h"
int wmain(int argc, wchar_t** argv) {
    try {
        auto strings = platform::utf8_arguments(argc, argv);
        std::vector<char*> arguments;
        arguments.reserve(strings.size() + 1);
        for (auto& value : strings) arguments.push_back(value.data());
        arguments.push_back(nullptr);
        return run(argc, arguments.data());
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
#else
int main(int argc, char** argv) { return run(argc, argv); }
#endif
