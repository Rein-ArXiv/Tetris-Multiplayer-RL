#pragma once
#include "meta/profile_wire.h"
#include <vector>
namespace study_meta {
struct ShopView {
    PublicProfile profile;
    std::string selected;
    std::vector<std::string> owned;
};
enum class ShopStatus { ok, unknown_account, unknown_icon, already_owned, insufficient_bp, not_owned, storage_error };
struct ShopResult { ShopStatus status; std::optional<ShopView> view; };
inline Json shop_json(const ShopView& view) {
    auto j=profile_json(view.profile);
    j["selected_icon_id"]=view.selected;j["owned"]=view.owned;
    return j;
}
} // namespace study_meta
