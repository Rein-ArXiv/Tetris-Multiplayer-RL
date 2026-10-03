// Text harness around the real C++ implementation, used by check_learning_framing.py.
#include "net/framing.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;if(s=="-") return out;
    for(size_t i=0;i<s.size();i+=2) out.push_back(static_cast<uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));
    return out;
}
std::string hex(const std::vector<uint8_t>& v) {
    if(v.empty()) return "-";
    std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(auto b:v) out<<std::setw(2)<<static_cast<unsigned>(b);
    return out.str();
}
int main() {
    std::string line;
    while(std::getline(std::cin,line)) {
        std::istringstream input(line);std::string command;input>>command;
        if(command=="B") {
            unsigned type;std::string data;input>>type>>data;
            std::cout<<hex(net::build_frame(static_cast<net::MsgType>(type),unhex(data)))<<'\n';
        } else {
            std::vector<uint8_t> bytes;std::string chunk;
            while(input>>chunk) {
                auto more=unhex(chunk);bytes.insert(bytes.end(),more.begin(),more.end());std::vector<net::Frame> out;
                const bool ok=net::parse_frames(bytes,out);
                std::cout<<(ok?'T':'F')<<':'<<hex(bytes);
                for(const auto& frame:out) std::cout<<':'<<static_cast<unsigned>(frame.type)<<','<<hex(frame.payload);
                std::cout<<' ';
                if(!ok) break;
            }
            std::cout<<'\n';
        }
    }
}
