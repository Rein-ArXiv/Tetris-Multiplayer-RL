#include "bindings/session.h"
#include <stdexcept>

void require(bool condition) {
    if (!condition) throw std::runtime_error("Session combat contract failed");
}
int main() {
    study_python::Session a(11);
    const auto before = a.state_bytes();
    bool rejected = false;
    try { a.add_garbage(-1); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && a.state_bytes() == before);
    auto b = a.clone();
    b.add_garbage(2);
    require(a.pending_garbage() == 0 && b.pending_garbage() == 2);
    require(a.grid() == b.grid());
    b.apply_action(b.legal_actions().front());
    require(b.pending_garbage() == 0 && b.last_garbage() == 2);
    b.add_garbage(study_grid::Grid::kRows);
    b.apply_action(b.legal_actions().front());
    require(b.finished());
    const auto ended = b.state_bytes();
    rejected = false;
    try { b.add_garbage(1); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected && b.state_bytes() == ended);
}
