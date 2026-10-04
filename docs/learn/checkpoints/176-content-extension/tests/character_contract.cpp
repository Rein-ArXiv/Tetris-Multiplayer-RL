#include "bot/characters.h"
#include "bot/paced_driver.h"
#include <sstream>
#include <iostream>
void require(bool ok){if(!ok)throw std::runtime_error("character contract");}
int main() {
    std::istringstream source("\xef\xbb\xbf" "a|Alpha|shared.onnx|a.png|portrait.png|Calm|3|2|8 # comment\n"
        "b|Beta|shared.onnx|||Quick|1|0|1\n");
    auto catalog=study_characters::Catalog::read(source);
    const auto chosen=catalog.select("a");
    require(chosen.id=="a" && chosen.behavior.model==catalog.select("b").behavior.model);
    auto repaint=chosen;repaint.appearance={"Renamed","new.png","new-portrait.png","New label"};
    study_python::Session first(91),second(91);
    study_paced::Driver one(chosen.behavior.pacing),two(repaint.behavior.pacing);
    auto pick=[](const auto& state,int& action){auto legal=state.legal_actions();if(legal.empty())return false;action=legal.front();return true;};
    for(int tick=0;tick<120 && !first.finished();++tick) {
        require(one.tick(first,pick).mask==two.tick(second,pick).mask);
        require(first.state_bytes()==second.state_bytes());
    }
    std::istringstream changed("a|Changed|other.onnx|||Label|1|0|1\n");
    catalog=study_characters::Catalog::read(changed);
    require(chosen.appearance.name=="Alpha" && chosen.behavior.model=="shared.onnx");
    for(const auto& text:{"a|A|x|||N|1|0|1\na|B|y|||N|1|0|1\n","a|A|x|||N|0|0|1\n","# empty\n"}) {
        bool rejected=false;std::istringstream bad(text);
        try{catalog=study_characters::Catalog::read(bad);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && catalog.select("a").appearance.name=="Changed");
    }
    bool unknown=false;try{catalog.select("Alpha");}catch(const std::out_of_range&){unknown=true;}
    require(unknown);
    std::cout<<"identity, shared model, appearance independence, selection snapshot and failed reload passed\n";
}
