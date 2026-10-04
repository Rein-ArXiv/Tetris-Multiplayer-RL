#include "meta/sqlite_results.h"
#include <charconv>
#include <cstring>
#include <iostream>
#include <filesystem>
int main(int argc, char** argv) {
    try {
        if (argc != 4) return 2;
        std::uint64_t player = 0;
        unsigned limit = 0;
        const auto p = std::from_chars(argv[2], argv[2] + std::strlen(argv[2]), player);
        const auto n = std::from_chars(argv[3], argv[3] + std::strlen(argv[3]), limit);
        if (p.ec != std::errc{} || *p.ptr != '\0' || n.ec != std::errc{} || *n.ptr != '\0') return 2;
        if (player == 0 || limit == 0 || limit > 50 || !std::filesystem::is_regular_file(argv[1])) return 2;
        study_meta::SqliteResults store(argv[1]);
        for (const auto& row : store.recent(player, limit))
            std::cout << "row=" << row.row << " key=" << row.key << '\n';
    } catch (const std::exception& e) {
        std::cerr << "history: " << e.what() << '\n';
        return 1;
    }
}
