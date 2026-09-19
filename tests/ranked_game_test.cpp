#include "../server/ranked_game.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

void require(bool value) {
    if (!value)
        throw std::runtime_error("ranked game contract failed");
}
std::vector<uint8_t> input(uint32_t from, const std::vector<uint8_t> &masks) {
    std::vector<uint8_t> result;
    net::le_write_u32(result, from);
    net::le_write_u16(result, static_cast<uint16_t>(masks.size()));
    result.insert(result.end(), masks.begin(), masks.end());
    return result;
}
void feed(relay::RankedGame &game, int side, uint32_t tick, uint8_t mask,
          relay::RankedGame::Clock::time_point now) {
    const auto bytes = input(tick, {mask});
    game.observe(side, net::MsgType::INPUT, bytes.data(), bytes.size(), now);
}
int main(int argc, char **argv) {
    const uint64_t seed = argc == 2 ? std::strtoull(argv[1], nullptr, 10) : 12345;
    relay::RankedGame game(seed);
    const auto now = relay::RankedGame::Clock::now();
    uint32_t ticks = 0;
    while (game.result().status == net::ResultStatus::Incomplete && ticks < 120) {
        feed(game, 1, ticks, INPUT_NONE, now);
        feed(game, 2, ticks, INPUT_DROP, now);
        ++ticks;
    }
    const auto result = game.result();
    require(result.status == net::ResultStatus::Applied && result.winner == 1);
    if (argc == 2) {
        std::cout << "{\"ticks\":" << ticks << ",\"score_a\":" << result.score_a
                  << ",\"score_b\":" << result.score_b << ",\"lines_a\":" << result.lines_a
                  << ",\"lines_b\":" << result.lines_b << "}\n";
        return 0;
    }
    // Claiming a win without any input can never produce an applied result.
    relay::RankedGame empty(seed);
    require(empty.result().status == net::ResultStatus::Incomplete);
    // Duplicate delivery is harmless; rewriting a delivered input is not.
    relay::RankedGame duplicate(seed);
    feed(duplicate, 1, 0, 0, now);
    feed(duplicate, 1, 0, 0, now);
    require(duplicate.result().status == net::ResultStatus::Incomplete);
    feed(duplicate, 1, 0, INPUT_DROP, now);
    require(duplicate.result().status == net::ResultStatus::InvalidReplay);
    for (auto bad :
         {input(1, {0}), input(0, {32}), input(UINT32_MAX, {0}), input(0, std::vector<uint8_t>(121))}) {
        relay::RankedGame invalid(seed);
        invalid.observe(1, net::MsgType::INPUT, bad.data(), bad.size(), now);
        require(invalid.result().status == net::ResultStatus::InvalidReplay);
    }
    relay::RankedGame fast(seed);
    for (unsigned i = 0; i < 181; ++i)
        feed(fast, 1, i, 0, now);
    require(fast.result().status == net::ResultStatus::InvalidReplay);
    relay::RankedGame changedSeed(seed);
    std::vector<uint8_t> seedPayload;
    net::le_write_u64(seedPayload, seed + 1);
    net::le_write_u32(seedPayload, 120);
    seedPayload.insert(seedPayload.end(), {2, 1});
    changedSeed.observe(1, net::MsgType::SEED, seedPayload.data(), seedPayload.size(), now);
    require(changedSeed.result().status == net::ResultStatus::InvalidReplay);
}
