#include "../meta/protocol.h"
#include <stdexcept>
void require(bool value) {
    if (!value)
        throw std::runtime_error("JSON input contract failed");
}
int main() {
    using namespace meta;
    for (const auto *bad : {"{\"token\":\"abc", "{\"n\":123garbage}", "{\"token\":\"a\",\"token\":\"b\"}",
                            "{\"token\":1,\"toke\\u006e\":2}", "[]", "{\"b\":truejunk}", "{\"n\":+1}"})
        require(!json_input::object(bad));
    require(proto::find_string("{\"nested\":{\"token\":\"abc\"}}", "token").empty());
    require(proto::find_string("{\"toke\\u006e\":\"abc\"}", "token") == "abc");
    require(!proto::find_int("{\"n\":9223372036854775808}", "n"));
    require(proto::find_int("{\"n\":-9223372036854775808}", "n") == INT64_MIN);
    require(!proto::find_int("{\"n\":1.5}", "n"));
    std::string nested = "{}";
    for (int i = 0; i < 20; ++i)
        nested = "{\"a\":" + nested + "}";
    require(!json_input::object(nested));
}
