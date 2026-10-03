// The checker links either the old extracted parser or the current pure helper.
#include "meta/http_client.h"
#include "meta/json_input.h"
#include <cstdio>
#include <limits>
std::optional<meta::client::MatchResult> parse_response(const std::string&);
int main(){
    using Json=meta::json_input::Json;
    Json valid={{"match_id",7},{"a",{{"elo_before",10},{"elo_after",15},{"delta",5}}},{"b",{{"elo_before",4},{"elo_after",0},{"delta",-4}}}};
    unsigned failures=0,checks=0;
    auto check=[&](const char* label,const std::string& body,bool wanted){++checks;const auto r=parse_response(body);if(bool(r)!=wanted){++failures;std::fprintf(stderr,"%s: got %d expected %d\n",label,bool(r),wanted);}if(r && wanted && (r->match_id!=7 || r->a.delta!=5 || r->b.delta!=-4)){++failures;std::fprintf(stderr,"%s: wrong values\n",label);}};
    check("compact",valid.dump(),true);check("whitespace",valid.dump(2),true);
    auto extended=valid;extended["a"]["note"]="brace } and quote \"a\":{";extended["a"]["future"]={{"x",1}};
    check("extra nested/string",extended.dump(),true);
    // Valid top-level JSON, but a,b are only present inside another object.
    check("wrong parent",Json{{"match_id",7},{"wrapper",{{"a",valid["a"]},{"b",valid["b"]}}}}.dump(),false);
    for(const auto* side:{"a","b"})for(const auto* field:{"elo_before","elo_after","delta"}){
        for(const Json& bad:{Json("5"),Json(1.25),Json(nullptr),Json(true),Json(std::numeric_limits<std::uint64_t>::max()),Json(std::int64_t(std::numeric_limits<int>::max())+1),Json(std::int64_t(std::numeric_limits<int>::min())-1)}){
            auto j=valid;j[side][field]=bad;check("field type/range",j.dump(),false);
        }
        auto j=valid;j[side].erase(field);check("missing field",j.dump(),false);
    }
    for(const Json& bad:{Json(0),Json(-1),Json(1.5),Json("7"),Json(std::numeric_limits<std::uint64_t>::max())}){auto j=valid;j["match_id"]=bad;check("id",j.dump(),false);}
    auto negative=valid;negative["a"]["elo_before"]=-1;negative["a"]["delta"]=16;check("negative RP",negative.dump(),false);
    auto inconsistent=valid;inconsistent["a"]["delta"]=6;check("inconsistent delta",inconsistent.dump(),false);
    auto source=valid.dump();check("trailing",source+"x",false);check("duplicate",source.substr(0,source.size()-1)+",\"match_id\":8}",false);
    check("oversize",std::string(65537,' ')+source,false);
    std::printf("match response: %u checks, %u failures\n",checks,failures);return failures?1:0;
}
