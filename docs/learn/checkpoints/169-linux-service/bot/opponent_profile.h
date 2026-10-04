#pragma once
#include "opponents.h"
#include <charconv>
#include <optional>
#include <string_view>
#include <utility>

namespace bot {
struct ProfileLine {
    std::optional<Opponent> entry;
    std::string error; // Empty entry + empty error means a blank/comment line.
};
namespace profile_detail {
inline std::string trim(std::string_view text) {
    const auto first=text.find_first_not_of(" \t\r\n");
    return first==text.npos ? "" : std::string(text.substr(first,text.find_last_not_of(" \t\r\n")-first+1));
}
inline bool text_field(const std::string& field) {
    for(unsigned char ch:field)if(ch<32 || ch==127)return false;
    return true;
}
inline bool number(const std::string& text,int& out,int low,int high) {
    if(text.empty() || text.find_first_not_of("0123456789")!=text.npos)return false;
    int value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<low || value>high)return false;
    out=value;return true;
}
}
// Parse trusted local profile syntax without loading models/images or mutating a roster.
inline ProfileLine parse_opponent_profile(std::string_view line) {
    if(line.substr(0,3)=="\xef\xbb\xbf")line.remove_prefix(3);
    line=line.substr(0,line.find('#'));
    if(profile_detail::trim(line).empty())return {};
    std::vector<std::string> fields;
    for(;;) {
        const auto end=line.find('|');fields.push_back(profile_detail::trim(line.substr(0,end)));
        if(end==line.npos)break;
        line.remove_prefix(end+1);
    }
    if(fields.size()!=9)return {{},"expected profile fields"};
    for(const auto& field:fields)if(!profile_detail::text_field(field))return {{},"control character"};
    const auto& id=fields[0];
    if(id.empty() || id.size()>32 || id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=id.npos)
        return {{},"invalid identity"};
    if(fields[1].empty() || fields[1].size()>96)return {{},"invalid name"};
    if(fields[2].empty())return {{},"empty model"};
    Opponent entry;
    if(!profile_detail::number(fields[6],entry.inputIntervalTicks,Pacing::interval_min,Pacing::interval_max) ||
       !profile_detail::number(fields[7],entry.thinkTicks,Pacing::think_min,Pacing::think_max) ||
       !profile_detail::number(fields[8],entry.minPieceTicks,Pacing::minimum_min,Pacing::minimum_max))
        return {{},"invalid pacing"};
    entry.id=id;entry.name=fields[1];entry.path=fields[2];
    entry.iconPath=fields[3];entry.portraitPath=fields[4];
    if(!fields[5].empty())entry.difficulty=fields[5];
    return {std::move(entry),{}};
}
}
