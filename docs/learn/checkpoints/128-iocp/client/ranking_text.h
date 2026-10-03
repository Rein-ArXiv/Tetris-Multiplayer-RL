#pragma once
#include "meta/ranking_wire.h"
#include <array>
namespace study_ranking_ui {
// Split ASCII decimal bytes for the 16-scalar text primitive, keeping every digit.
inline std::array<std::string,3> row_text(std::size_t index,const study_meta::RankRow& row) {
    const auto id=std::to_string(row.player_id);
    return {std::to_string(index+1)+" RP "+std::to_string(row.rp),"ID "+id.substr(0,10),id.size()>10 ? id.substr(10) : std::string{}};
}
} // namespace study_ranking_ui
