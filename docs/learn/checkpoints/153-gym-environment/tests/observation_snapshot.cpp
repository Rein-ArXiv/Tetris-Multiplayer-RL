#include "bindings/session.h"
#include <stdexcept>

void require(bool value) {if(!value)throw std::runtime_error("snapshot contract");}
int main() {
    study_python::Session a(77,2);
    auto b=a.clone();
    const auto before=a.state_bytes();
    auto snapshot=a.snapshot();
    require(snapshot.board.size()==study_grid::Grid::kRows);
    for(const auto& row:snapshot.board)require(row.size()==study_grid::Grid::kColumns);
    const auto ids=study_python::Session::piece_ids();
    require(ids.size()==study_catalog::definitions.size());
    for(std::size_t i=0;i<ids.size();++i)
        require(ids[i]==static_cast<int>(study_catalog::definitions[i].kind));
    snapshot.board[0][0]=99;
    require(a.snapshot().board[0][0]==0 && a.state_bytes()==before);
    b.step(0);
    require(a.snapshot().board==b.snapshot().board);
    require(a.snapshot().current_id==b.snapshot().current_id);
    require(a.state_bytes()!=b.state_bytes());
    for(int i=0;i<1000 && !a.finished();++i)a.step(study_input::drop);
    require(a.finished());
    const auto terminal=a.snapshot();
    require(terminal.current_id>0 && terminal.next_id>0);
}
