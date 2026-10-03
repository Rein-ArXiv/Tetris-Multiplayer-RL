#include "meta/http_client.h"
#include <cstdio>
#include <string>
int main(int argc, char** argv) {
    if (argc != 2 && argc != 3) return 2;
    meta::client::MetaClient api(argv[1]);
    if (!api.valid()) return 3;
    if (argc == 2) {
        const auto result = api.request_guest(1);
        if (!result) return 4;
        if (result->player_id != 135 || result->bp != 0) return 5;
    }
    const auto catalog = api.fetch_icon_catalog(1);
    if (!catalog) return 4;
    if (catalog->size() != 1 || catalog->front().id != "default") return 5;
    std::puts("HTTPS guest response verified");
}
