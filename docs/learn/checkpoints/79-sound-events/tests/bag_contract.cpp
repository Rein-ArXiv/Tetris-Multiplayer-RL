#include "simulation/seven_bag.h"
#include "simulation/round.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <set>
#include <vector>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "CHECK failed at %d: %s\n", __LINE__, #x); std::exit(1); } } while (false)
using Bag = study_bag::SevenBag;
using Kind = Bag::Kind;
using Pattern = std::array<Kind, 7>;
static std::set<Pattern> permutations;
static unsigned positions[7][8]{};
static unsigned paths = 0;

static std::vector<Kind> catalog() {
    std::vector<Kind> result;
    for (const auto& definition : study_catalog::definitions) result.push_back(definition.kind);
    return result;
}
static void compare(const Bag& bag, const std::vector<Kind>& oracle) {
    CHECK(bag.remaining() == oracle.size());
    CHECK(bag.next_bound() == (oracle.empty() ? 7 : oracle.size()));
    for (std::size_t i = 0; i < 8; ++i) {
        const auto expected = i < oracle.size() ? std::optional<Kind>{oracle[i]} : std::nullopt;
        CHECK(bag.at(i) == expected);
    }
    CHECK(!bag.at(std::numeric_limits<std::size_t>::max()));
}
static void check_round(const Pattern& pattern) {
    const auto source = study_next::ScriptedSource::from_pattern(pattern, pattern.size());
    CHECK(source);
    auto round = study_round::Round::create(study_grid::Grid{}, *source);
    CHECK(round && source->cursor() == 0);
    for (std::size_t locks = 0; locks <= 3; ++locks) {
        CHECK(round->kind() == pattern[locks]);
        CHECK(round->source_cursor() == (locks + 4) % 7);
        for (std::size_t i = 0; i < 3; ++i) CHECK(round->next().peek(i) == pattern[locks + i + 1]);
        if (locks < 3) CHECK(round->tick(0, false, false, true) == study_round::Step::locked);
    }
    CHECK(source->cursor() == 0);
}
static void enumerate(Bag bag, std::vector<Kind> oracle, Pattern pattern, std::size_t depth) {
    compare(bag, oracle);
    // Every prefix rejects invalid choices without changing its logical state.
    CHECK(!bag.take(bag.next_bound())); compare(bag, oracle);
    CHECK(!bag.take(std::numeric_limits<std::size_t>::max())); compare(bag, oracle);
    if (depth == 7) {
        CHECK(permutations.insert(pattern).second);
        for (std::size_t i = 0; i < 7; ++i) ++positions[i][static_cast<unsigned>(pattern[i])];
        ++paths;
        check_round(pattern);
        for (std::size_t index = 0; index < 7; ++index) {
            auto copy = bag;
            auto refilled = catalog();
            CHECK(copy.take(index) == refilled[index]);
            refilled.erase(refilled.begin() + static_cast<std::ptrdiff_t>(index));
            compare(copy, refilled);
            compare(bag, {}); // Modifying a copy does not refill the original.
        }
        return;
    }
    for (std::size_t index = 0; index < oracle.size(); ++index) {
        auto copy = bag;
        auto next = oracle;
        const Kind expected = next[index];
        next.erase(next.begin() + static_cast<std::ptrdiff_t>(index));
        const auto value = copy.take(index);
        CHECK(value == expected);
        pattern[depth] = *value;
        compare(copy, next);
        compare(bag, oracle);
        enumerate(copy, next, pattern, depth + 1);
    }
}
// Construct a valid permutation with I at a specified position, using the bag API.
static Pattern with_i(std::size_t position) {
    auto desired = catalog();
    desired.erase(desired.begin());
    desired.insert(desired.begin() + static_cast<std::ptrdiff_t>(position), Kind::I);
    Bag bag;
    Pattern result{};
    for (std::size_t i = 0; i < 7; ++i) {
        std::size_t index = 0;
        while (index < bag.remaining() && bag.at(index) != desired[i]) ++index;
        const auto value = bag.take(index);
        CHECK(value && *value == desired[i]);
        result[i] = *value;
    }
    return result;
}
static void boundaries() {
    int smallest = 99, largest = 0;
    for (std::size_t a = 0; a < 7; ++a) {
        for (std::size_t b = 0; b < 7; ++b) {
            auto first = with_i(a), second = with_i(b);
            std::vector<Kind> sequence(first.begin(), first.end());
            sequence.insert(sequence.end(), second.begin(), second.end());
            int between = 0;
            for (std::size_t i = a + 1; i < 7 + b; ++i) { CHECK(sequence[i] != Kind::I); ++between; }
            CHECK(between == 6 - static_cast<int>(a) + static_cast<int>(b));
            smallest = std::min(smallest, between); largest = std::max(largest, between);
        }
    }
    CHECK(smallest == 0 && largest == 12);
    unsigned min_count = 9, max_count = 0, max_run = 0;
    for (std::size_t a = 0; a < 7; ++a) for (std::size_t b = 0; b < 7; ++b) for (std::size_t c = 0; c < 7; ++c) {
        std::vector<Kind> sequence;
        for (auto position : {a, b, c}) {
            const auto part = with_i(position);
            sequence.insert(sequence.end(), part.begin(), part.end());
        }
        unsigned run = 0;
        for (const auto kind : sequence) {
            run = kind == Kind::I ? run + 1 : 0;
            max_run = std::max(max_run, run);
        }
        for (std::size_t start = 0; start < 7; ++start) {
            unsigned count = 0;
            for (std::size_t i = start; i < start + 14; ++i) count += sequence[i] == Kind::I;
            if (start == 0) CHECK(count == 2);
            CHECK(count >= 1 && count <= 3);
            min_count = std::min(min_count, count); max_count = std::max(max_count, count);
        }
    }
    CHECK(min_count == 1 && max_count == 3 && max_run == 2);
    unsigned counts[3]{};
    for (unsigned value = 0; value < 8; ++value) ++counts[value % 3];
    CHECK(counts[0] == 3 && counts[1] == 3 && counts[2] == 2);
}
int main() {
    enumerate(Bag{}, catalog(), Pattern{}, 0);
    CHECK(paths == 5040 && permutations.size() == 5040);
    for (const auto& row : positions) for (std::size_t id = 1; id <= 7; ++id) CHECK(row[id] == 720);
    boundaries();
    std::puts("5040 unique permutations; 5040 Round patterns/three promotions; stable erase, invalid/copy/refill contracts; 49 drought pairs and 2401 windows; toy modulo 3/3/2 passed");
}
