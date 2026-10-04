#include "bot/opponent_profile.h"
#include <iostream>
#include <iomanip>
int main() {
    std::string line;
    while(std::getline(std::cin,line)) {
        const auto result=bot::parse_opponent_profile(line);
        if(!result.error.empty())std::cout<<"error\n";
        else if(!result.entry)std::cout<<"skip\n";
        else {
            const auto& e=*result.entry;
            std::cout<<"ok "<<std::quoted(e.id)<<' '<<std::quoted(e.name)<<' '<<std::quoted(e.path)<<' '
                <<std::quoted(e.iconPath)<<' '<<std::quoted(e.portraitPath)<<' '<<std::quoted(e.difficulty)<<' '
                <<e.inputIntervalTicks<<' '<<e.thinkTicks<<' '<<e.minPieceTicks<<'\n';
        }
    }
}
