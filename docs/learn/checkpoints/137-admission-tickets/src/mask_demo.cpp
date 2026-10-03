// CPU-only demo: inspect the byte-mask input encoding.
#include <bitset>
#include <cstdint>
#include <iostream>

#include "simulation/input_mask.h"
#include "simulation/pending_controls.h"

namespace {

void show_mask(unsigned value) {
    std::cout << "raw=" << value;
    if (study_input::valid(value)) {
        std::cout << " bits=" << std::bitset<8>(value);
        const auto decoded = study_input::decode(value);
        std::cout << " h=" << decoded->horizontal
                  << " cw=" << decoded->clockwise
                  << " soft=" << decoded->soft_drop
                  << " hard=" << decoded->hard_drop;
    } else {
        std::cout << " invalid";
    }
    std::cout << '\n';
}

} // namespace

int main() {
    using namespace study_input;

    std::cout << "== mask values ==\n";
    for (unsigned value : {0u, 9u, 19u, 31u, 32u, 256u}) {
        show_mask(value);
    }

    std::cout << "== round trip ==\n";
    const auto both = decode(left | right);
    std::cout << "decode(left|right).horizontal=" << both->horizontal << '\n';
    std::cout << "encode(decode(left|right))="
              << static_cast<unsigned>(*encode(*both)) << '\n';

    for (int h : {-1, 0, 1}) {
        const auto mask = encode(Intent{h, true, true, true});
        const auto out = decode(static_cast<unsigned>(*mask));
        std::cout << "h=" << h << " mask=" << static_cast<unsigned>(*mask)
                  << " -> h=" << out->horizontal << '\n';
    }

    std::cout << "encode(h=2)="
              << (encode(Intent{2, false}).has_value() ? "value" : "nullopt")
              << '\n';

    std::cout << "== capture/consume ==\n";
    PendingControls controls;
    controls.capture(true, false, false);
    controls.capture(true, false, false);
    controls.capture(false, false, false, true);
    const Intent t1 = controls.consume();
    std::cout << "t1 h=" << t1.horizontal << " soft=" << t1.soft_drop << '\n';
    const Intent t2 = controls.consume();
    std::cout << "t2 h=" << t2.horizontal << " soft=" << t2.soft_drop << '\n';

    controls.capture(true, false, false, true);
    controls.clear();
    const Intent t3 = controls.consume();
    std::cout << "t3 h=" << t3.horizontal << " soft=" << t3.soft_drop << '\n';

    return 0;
}
