#include "meta/outbox_delivery.h"
#include "meta/outbox_http.h"
#include "tools/sample_match.h"
#include <charconv>
#include <cstring>
#include <iostream>
#include <thread>
template<class T> bool parse_arg(const char* text, T& value) {
    const auto end = text + std::strlen(text);
    const auto result = std::from_chars(text, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}
int main(int argc, char** argv) {
    if (argc < 4 || argc > 5) return 2;
    int port = 0;
    if (!parse_arg(argv[1], port) || port < 1 || port > 65535) return 2;
    const std::string command = argv[3];
    try {
        study_meta::OutboxHttp api(port);
        study_meta::LocalOutbox store(std::filesystem::u8path(argv[2]), api.origin());
        if (command == "prepare" && argc == 5) {
            std::uint64_t key = 0;
            if (!parse_arg(argv[4], key) || !key) return 2;
            const auto record = sample_match(key);
            if (!record || !store.prepare(*record)) {
                std::cout << "request not prepared; keep existing files\n"; return 5;
            }
            std::cout << "request prepared; run resume with the same folder and port\n"; return 0;
        }
        if (command != "resume" || argc != 4) return 2;
        study_meta::OutboxDelivery delivery(store);
        for (;;) {
            const auto result = delivery.deliver(api,
                [] { return std::chrono::steady_clock::now(); },
                [](std::chrono::milliseconds delay) { std::this_thread::sleep_for(delay); },
                std::chrono::milliseconds{5000});
            using S = study_meta::DeliveryState;
            switch (result.state) {
            case S::empty: std::cout << "no prepared result\n"; return 5;
            case S::storage_blocked:
                std::cout << "local request not ready; no new HTTP attempt\n"; return 5;
            case S::unconfirmed:
                std::cout << "server outcome unconfirmed; original request kept\n"; return 3;
            case S::stopped:
                std::cout << "automatic sending stopped; inspect the original request\n"; break;
            case S::confirmed:
                std::cout << "server confirmed key=" << result.receipt->key
                          << " row=" << result.receipt->row << '\n'; break;
            }
            if (result.local_saved) return result.state == S::confirmed ? 0 : 4;
            std::cout << "local receipt/status save unconfirmed; keep this process open. Enter retry or quit\n";
            std::string input;
            if (!std::getline(std::cin, input) || input != "retry") return 6;
        }
    } catch (const std::exception&) {
        std::cerr << "outbox unavailable; keep files for inspection\n"; return 1;
    }
}
