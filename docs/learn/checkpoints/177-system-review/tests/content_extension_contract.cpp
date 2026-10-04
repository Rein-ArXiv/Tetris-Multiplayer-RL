#include "client/opponent_cards.h"
#include "bot/practice_picker.h"
#include "bot/policy_match.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
void require(bool ok) { if (!ok) throw std::runtime_error("content extension contract"); }
static int run(int argc, char** argv) {
    require(argc==2);
    std::ifstream input(std::filesystem::u8path(argv[1])); require(bool(input));
    auto catalog = study_characters::Catalog::read(input);
    const auto chosen = catalog.select("mira");
    const auto cards = study_opponent_menu::cards(catalog);
    require(chosen.appearance.name == u8"미라" && chosen.behavior.model == "@heuristic");
    require(cards.size() == catalog.entries().size());
    const auto found = std::find_if(cards.begin(), cards.end(), [](const auto& c) { return c.id=="mira"; });
    require(found != cards.end() && found->name==chosen.appearance.name);
    auto repaint = chosen; repaint.appearance = {"Renamed", "other.png", "portrait.png", "Different label"};
    study_match::Match one(77, chosen, bot::Mode::practice), two(77, repaint, bot::Mode::practice);
    bool sawDecision = false, sawInput = false;
    for (int tick=0; tick<150 && !one.finished(); ++tick) {
        const auto human = tick%17==16 ? study_input::drop : 0;
        const auto a=one.tick(human,study_practice::pick,false);
        const auto b=two.tick(human,study_practice::pick,false);
        require(a.bot.mask==b.bot.mask && a.source==b.source);
        require(one.human().state_bytes()==two.human().state_bytes());
        require(one.enemy().state_bytes()==two.enemy().state_bytes());
        require(!one.status().reward_eligible() && !two.status().reward_eligible());
        sawDecision |= a.bot.decision_attempted; sawInput |= a.bot.mask!=0;
    }
    require(sawDecision && sawInput);
    // Parse failure preserves the live roster because replacement happens after construction.
    bool rejected=false;std::istringstream duplicate("mira|A|@heuristic|||N|1|0|1\nmira|B|@heuristic|||N|1|0|1\n");
    try { catalog=study_characters::Catalog::read(duplicate); }
    catch(const std::invalid_argument&) { rejected=true; }
    require(rejected && catalog.select("mira").appearance.name==u8"미라");
    std::istringstream replacement("mira|Changed|@heuristic|||N|1|0|1\n");
    catalog=study_characters::Catalog::read(replacement);
    require(chosen.appearance.name==u8"미라" && found->name==u8"미라");
    bool unknown=false;try { catalog.select("미라"); } catch(const std::out_of_range&) {unknown=true;}
    require(unknown);
    std::cout<<"New stable ID, owned cards, paced practice, repaint invariance and failed reload passed\n";
    return 0;
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
