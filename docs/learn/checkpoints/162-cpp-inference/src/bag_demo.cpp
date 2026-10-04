#include "simulation/seven_bag.h"
#include "simulation/round.h"

#include <cstddef>
#include <iostream>

namespace study_demo {

using study_bag::SevenBag;
using Kind = SevenBag::Kind;

void print_kind(Kind kind) {
    // Kind values run 1..7, not array indices 0..6.
    std::cout << study_catalog::find(kind)->name;
}

void print_sequence(const char* label, SevenBag& bag, bool first_only,
                    std::size_t draws) {
    std::cout << label << ":";
    for (std::size_t i = 0; i < draws; ++i) {
        const std::size_t index = first_only ? 0 : bag.next_bound() - 1;
        const auto kind = bag.take(index);
        std::cout << ' ';
        if (kind) {
            print_kind(*kind);
        } else {
            std::cout << "<null>";
        }
    }
    std::cout << '\n';
}

void run() {
    // First-only draws consume the head; each refill restores factory order.
    SevenBag head;
    print_sequence("first-only x9", head, true, 9);

    // Last-only draws consume the tail; each refill reverses the visible order.
    SevenBag tail;
    print_sequence("last-only x9", tail, false, 9);

    // Empty state reports remaining 0 and next_bound 7.
    SevenBag probe;
    for (std::size_t i = 0; i < SevenBag::capacity; ++i) {
        (void)probe.take(0);
    }
    std::cout << "empty: remaining=" << probe.remaining()
              << " next_bound=" << probe.next_bound() << '\n';

    // An invalid draw at empty returns null and preserves the empty state.
    const auto rejected = probe.take(7);
    std::cout << "take(7) at empty -> " << (rejected ? "kind" : "null")
              << ", remaining=" << probe.remaining() << '\n';

    // Own a generated permutation in the existing Round supply path.
    // ScriptedSource repeats this one pattern; each refill is not a new shuffle.
    SevenBag supply;
    std::array<Kind, 7> pattern{};
    const std::array<std::size_t, 7> choices{3, 0, 4, 1, 2, 0, 0};
    for (std::size_t i = 0; i < pattern.size(); ++i) {
        const auto drawn = supply.take(choices[i]);
        if (!drawn) return; // All deliberate choices above are in range.
        pattern[i] = *drawn;
    }
    const auto source = study_next::ScriptedSource::from_pattern(pattern, pattern.size());
    if (!source) return;
    auto round = study_round::Round::create(study_grid::Grid{}, *source);
    if (!round) return;
    std::cout << "round current=";
    print_kind(round->kind());
    std::cout << " preview=";
    for (std::size_t i = 0; i < 3; ++i) print_kind(*round->next().peek(i));
    std::cout << " cursor=" << round->source_cursor() << '\n';
    for (int step = 0; step < 3; ++step) {
        (void)round->tick(0, false, false, true);
    }
    std::cout << "after 3 locks current=";
    print_kind(round->kind());
    std::cout << " preview=";
    for (std::size_t i = 0; i < 3; ++i) print_kind(*round->next().peek(i));
    std::cout << " cursor=" << round->source_cursor() << '\n';

    // Repeated bag boundary: drain one full bag, then begin the next.
    SevenBag boundary;
    std::cout << "boundary:";
    for (std::size_t i = 0; i < 10; ++i) {
        if (i == 7) {
            std::cout << " |";
        }
        const auto kind = boundary.take(0);
        std::cout << ' ';
        if (kind) {
            print_kind(*kind);
        } else {
            std::cout << "<null>";
        }
    }
    std::cout << '\n';

    // Toy modulo table: uniformly enumerated 0..7 onto three choices.
    // Deliberate toy test input, not an actual RNG.
    unsigned counts[3] = {0, 0, 0};
    for (unsigned sample = 0; sample < 8; ++sample) {
        counts[sample % 3] += 1;
    }
    std::cout << "toy counts=" << counts[0] << '/' << counts[1] << '/'
              << counts[2] << " (toy, not actual RNG)\n";
}

}  // namespace study_demo

int main() {
    study_demo::run();
    return 0;
}
