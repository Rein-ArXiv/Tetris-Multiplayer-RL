#include "bot/placement.h"
#include <iostream>

int main() {
    int column,quarter,target_column,target_quarter;
    while(std::cin>>column>>quarter>>target_column>>target_quarter) {
        if(column<0 || column>=bot::kNumCols || target_column<0 || target_column>=bot::kNumCols ||
           quarter<0 || quarter>=bot::kNumRotations || target_quarter<0 || target_quarter>=bot::kNumRotations)
            return 1;
        const auto encoded=bot::encode_action(target_column,target_quarter);
        int decoded_column=0,decoded_quarter=0;
        bot::decode_action(encoded,decoded_column,decoded_quarter);
        std::cout<<encoded<<' '<<decoded_column<<' '<<decoded_quarter;
        for(const auto mask:bot::expand_placement(column,quarter,target_column,target_quarter))
            std::cout<<' '<<unsigned(mask);
        std::cout<<'\n';
    }
    return std::cin.eof()?0:1;
}
