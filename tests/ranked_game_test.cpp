#include "../server/ranked_game.h"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
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
// Compare the relay wrapper against direct execution of the shared rule core.
// The oracle has no network batching or RankedGame result calculation.
void direct_replay_contract(uint64_t seed, uint8_t host_mask, uint8_t peer_mask,
                            std::size_t batch_size, bool peer_first) {
    SimGame host(seed), peer(seed);
    std::vector<uint8_t> host_inputs, peer_inputs;
    int delivered_host = 0, delivered_peer = 0;
    constexpr std::size_t test_limit = 1000;
    while (!host.IsGameOver() && !peer.IsGameOver()) {
        require(host_inputs.size() < test_limit);
        host.SubmitInput(host_mask);
        peer.SubmitInput(peer_mask);
        host.Tick();
        peer.Tick();
        const int host_total = host.AttackLinesSent(), peer_total = peer.AttackLinesSent();
        peer.AddPendingGarbage(host_total - delivered_host);
        host.AddPendingGarbage(peer_total - delivered_peer);
        delivered_host = host_total;
        delivered_peer = peer_total;
        host_inputs.push_back(host_mask);
        peer_inputs.push_back(peer_mask);
    }
    relay::RankedGame replay(seed);
    const auto began = relay::RankedGame::Clock::now();
    for (std::size_t offset = 0; offset < host_inputs.size(); offset += batch_size) {
        const auto count = std::min(batch_size, host_inputs.size() - offset);
        const auto a = input(static_cast<uint32_t>(offset),
                             {host_inputs.begin() + offset, host_inputs.begin() + offset + count});
        const auto b = input(static_cast<uint32_t>(offset),
                             {peer_inputs.begin() + offset, peer_inputs.begin() + offset + count});
        const auto now = began + std::chrono::seconds(offset);
        const auto& first = peer_first ? b : a;
        const auto& second = peer_first ? a : b;
        replay.observe(peer_first ? 2 : 1, net::MsgType::INPUT, first.data(), first.size(), now);
        replay.observe(peer_first ? 1 : 2, net::MsgType::INPUT, second.data(), second.size(), now);
    }
    const auto verified = replay.result();
    const int winner = host.IsGameOver() == peer.IsGameOver() ? 0 : host.IsGameOver() ? 2 : 1;
    require(verified.winner == winner);
    require(verified.status == (winner ? net::ResultStatus::Applied : net::ResultStatus::Draw));
    require(verified.score_a == host.Score() && verified.score_b == peer.Score());
    require(verified.lines_a == host.totalLinesCleared && verified.lines_b == peer.totalLinesCleared);
}

// Current policy fixtures: elapsed time is supplied by the server clock.
// Verify the boundary on each side without waiting or changing the game rules.
void pacing_contract(uint64_t seed) {
    using Game = relay::RankedGame;
    const Game::Clock::time_point began{};
    constexpr uint32_t fixture_lead = 180, fixture_rate = 60, fixture_window = 4096;
    auto fill = [](Game& game, uint32_t first, uint32_t end, Game::Clock::time_point at) {
        while (first < end) {
            const auto count = std::min(uint32_t(120), end - first);
            const auto bytes = input(first, std::vector<uint8_t>(count, INPUT_NONE));
            game.observe(1, net::MsgType::INPUT, bytes.data(), bytes.size(), at);
            first += count;
        }
    };
    for (bool early : {false, true}) {
        Game game(seed);
        fill(game, 0, fixture_lead, began);
        require(game.result().status == net::ResultStatus::Incomplete);
        const auto at = began + std::chrono::seconds(1) - std::chrono::milliseconds(early ? 1 : 0);
        fill(game, fixture_lead, fixture_lead + fixture_rate, at);
        require(game.result().status == (early ? net::ResultStatus::InvalidReplay
                                              : net::ResultStatus::Incomplete));
    }
    Game fixed_start(seed);
    fill(fixed_start, 0, fixture_lead, began);
    // Repeated packets do not create a new initial lead allowance.
    feed(fixed_start, 1, 0, INPUT_NONE, began);
    feed(fixed_start, 1, fixture_lead, INPUT_NONE, began);
    require(fixed_start.result().status == net::ResultStatus::InvalidReplay);
    Game window(seed);
    feed(window, 1, 0, INPUT_NONE, began);
    fill(window, 1, fixture_window, began + std::chrono::seconds(100));
    require(window.result().status == net::ResultStatus::Incomplete);
    feed(window, 1, fixture_window, INPUT_NONE, began + std::chrono::seconds(100));
    require(window.result().status == net::ResultStatus::InvalidReplay);
    for (auto bad : {input(Game::max_ticks, {0}), input(Game::max_ticks - 1, {0, 0}),
                     input(0, {})}) {
        Game game(seed);
        game.observe(1, net::MsgType::INPUT, bad.data(), bad.size(), began);
        require(game.result().status == net::ResultStatus::InvalidReplay);
    }
    auto incomplete = input(0, {0});incomplete.pop_back();
    Game malformed(seed);
    malformed.observe(1, net::MsgType::INPUT, incomplete.data(), incomplete.size(), began);
    require(malformed.result().status == net::ResultStatus::InvalidReplay);
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
    for (uint64_t replay_seed : std::array<uint64_t, 4>{0, 77, 12345, UINT64_MAX}) {
        for (const auto masks : {std::pair<uint8_t, uint8_t>{INPUT_NONE, INPUT_DROP},
                                 {INPUT_DROP, INPUT_NONE}, {INPUT_DROP, INPUT_DROP}}) {
            for (std::size_t batch : {std::size_t(1), std::size_t(7)}) {
                direct_replay_contract(replay_seed, masks.first, masks.second, batch, false);
                direct_replay_contract(replay_seed, masks.first, masks.second, batch, true);
            }
        }
    }
    pacing_contract(seed);
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
    // Exhaust every byte: direction policy is separate from unknown-bit validation.
    for(unsigned mask=0;mask<256;++mask) {
        relay::RankedGame checked(seed);
        feed(checked,1,0,static_cast<uint8_t>(mask),now);
        require((checked.result().status==net::ResultStatus::InvalidReplay)==(mask>=32));
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
