#include "meta/json_input.h"
#include "meta/match_response.h"

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

std::string nested(int n) {
    std::string s="0";
    while(n--) s="{\"x\":"+s+"}";
    return s;
}
int main() {
    using namespace meta::json_input;
    document_contract([](const std::string& s){return object(s);},64*1024);
    CHECK(object(nested(16))); CHECK(!object(nested(17)));
    CHECK(object(R"({"a":{"x":1},"b":{"x":2},"x":3})"));
    CHECK(object(R"({"a":[{"x":1},{"x":2}]})"));
    CHECK(!object(R"({"a":[{"x":1,"\u0078":2}]})"));
    CHECK(!object(R"({"a":{"x":1},"a":{"x":2}})"));
    CHECK(integer(R"({"n":9223372036854775807})","n") == INT64_MAX);
    CHECK(integer(R"({"n":-9223372036854775808})","n") == INT64_MIN);
    for(const auto* literal : {"9223372036854775808","-9223372036854775809","1.0","1e0","true","null","\"1\""})
        CHECK(!integer(std::string("{\"n\":")+literal+"}","n"));
    CHECK(!integer(R"({"child":{"n":1}})","n"));
    CHECK(boolean(R"({"b":false})","b") == false);
    CHECK(!boolean(R"({"b":0})","b"));
    CHECK(string(R"({"s":"a\u0000b"})","s") == std::string("a\0b",3));
    CHECK(string(R"({"s":false})","s").empty());
    const std::string response=R"({"match_id":1,"a":{"elo_before":0,"elo_after":16,"delta":16},"b":{"elo_before":0,"elo_after":0,"delta":0}})";
    CHECK(meta::client::parse_match_response(response));
    CHECK(!meta::client::parse_match_response(response+'\0'+"tail"));
    std::cout << "Actual JSON parser: byte/depth/sibling scope/types/NUL and response boundary passed\n";
}
