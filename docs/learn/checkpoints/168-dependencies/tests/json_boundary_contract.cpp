#include "meta/shop_catalog.h"
#include "meta/profile_wire.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#define CHECK(x) do { if (!(x)) { std::cerr << "failed line " << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)

template<class Parse>
void document_contract(Parse parse, std::size_t max_bytes) {
    CHECK(parse("{}"));
    CHECK(parse(" \t{\"x\":1}\r\n"));
    for (const auto* body : {"", "null", "[]", "1", "true", "{}{}", "{}garbage",
         "{\"x\":01}", "{\"x\":1,}", "{\"x\":NaN}", "{\"x\":1e999}", "{/*x*/}",
         "{\"x\":1,\"x\":1}", "{\"x\":1,\"\\u0078\":2}",
         "{\"x\":\"\\uD800\"}", "{\"x\":\"\\uDC00\"}", "{\"x\":\"unfinished}"})
        CHECK(!parse(body));
    const std::string good = R"({"x":"ruby"})";
    for (std::size_t i = 0; i <= good.size(); ++i) {
        auto body = good; body.insert(i, 1, '\0');
        CHECK(!parse(body));
    }
    CHECK(!parse(good + '\0' + "ignored"));
    for (int c = 0; c < 32; ++c) {
        CHECK(!parse(std::string("{\"x\":\"") + char(c) + "\"}"));
        if (c != 9 && c != 10 && c != 13) CHECK(!parse(good + char(c)));
    }
    for (auto bad : {std::string("\xC0\xAF",2), std::string("\xED\xA0\x80",3), std::string("\xFF",1)})
        CHECK(!parse(std::string("{\"x\":\"") + bad + "\"}"));
    CHECK(parse(R"({"x":"한글\uD83D\uDE00"})"));
    auto escaped = parse(R"({"x":"a\u0000b"})");
    CHECK(escaped && (*escaped)["x"].template get<std::string>() == std::string("a\0b",3));
    CHECK(parse("{}" + std::string(max_bytes - 2, ' ')));
    CHECK(!parse("{}" + std::string(max_bytes - 1, ' ')));
}

int main() {
    using namespace study_meta;
    // A sentinel count follows the known test cases, not an unvalidated parse.
    document_contract([](const std::string& s) {
        // General fixture allows either 0 or 1 fields, while production callers
        // pass exactly the endpoint's field count.
        auto j = object(s, 0); return j ? j : object(s, 1);
    }, kJsonBodyBytes);
    CHECK(!object(R"({"x":[]})",1));
    CHECK(!object(R"({"x":{}})",1));
    CHECK(!object(R"({"x":1,"y":2})",1));
    CHECK(!object(R"({"x":1})",2));
    for (const auto* literal : {"-1", "1.0", "1e0", "true", "null", "\"1\"", "18446744073709551616"}) {
        auto j = object(std::string("{\"n\":")+literal+"}",1);
        CHECK(j && !number(*j,"n"));
    }
    auto j = object(R"({"n":18446744073709551615})",1);
    CHECK(j && number(*j,"n") == std::numeric_limits<std::uint64_t>::max());
    CHECK(number(Json{{"n",0}},"n") == 0);
    CHECK(number(Json::parse(R"({"n":-0})"),"n") == 0);
    CHECK(!number(Json{{"n",0}},"missing"));
    CHECK(!number(Json::array(),"n"));
    CHECK(!text(Json{{"x",1}},"x"));
    CHECK(!text(Json{{"x",nullptr}},"x"));
    CHECK(!text(Json{{"x",""}},"missing"));
    auto empty = text(Json{{"x",""}},"x"); CHECK(empty && empty->empty());
    auto nul = object(R"({"x":"a\u0000b"})",1);
    CHECK(nul && text(*nul,"x") == std::string("a\0b",3));
    CHECK(!parse_icon_choice(R"({"icon_id":"ruby\u0000suffix"})"));
    CHECK(!parse_icon_choice(R"({"icon_id":""})"));
    CHECK(!parse_icon_choice(R"({"other":"ruby"})"));
    CHECK(!parse_icon_choice(R"({"icon_id":"ruby","price":0})"));
    CHECK(parse_icon_choice(R"({"icon_id":"unknown"})") == "unknown");
    CHECK(shop_icon("unknown") == nullptr);
    CHECK(parse_icon_choice(" { \"icon_\\u0069d\" : \"ruby\" } ") == "ruby");
    CHECK(!parse_icon_choice(R"({"icon_id":"ruby","icon_\u0069d":"gold"})"));
    CHECK(parse_profile(R"({"player_id":18446744073709551615,"rp":0,"xp":0,"bp":2147483647})"));
    CHECK(!parse_profile(R"({"player_id":1,"rp":0,"xp":0,"bp":2147483648})"));
    const auto record = Json{{"key",1},{"round",1},{"player_a",101},{"player_b",202},
        {"ticks",3},{"score_a",0},{"score_b",0},{"lines_a",0},{"lines_b",0},{"winner",0}};
    CHECK(parse_record(record.dump()));
    CHECK(!parse_record(record.dump() + '\0' + "tail"));
    auto bad_record = record; bad_record["lines_a"] = 4294967296ULL;
    CHECK(!parse_record(bad_record.dump()));
    std::cout << "JSON document/types/ranges/UTF-8/NUL/duplicate/schema contracts: passed\n";
}
