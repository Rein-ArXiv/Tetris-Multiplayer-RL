#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "bindings/session.h"

namespace py = pybind11;

PYBIND11_MODULE(study_py, m) {
    m.doc() = "Owned Python calls into the cumulative C++ Round";
    py::enum_<study_round::Step>(m, "Step")
        .value("waiting", study_round::Step::waiting)
        .value("changed", study_round::Step::changed)
        .value("locked", study_round::Step::locked)
        .value("game_over", study_round::Step::game_over)
        .value("stopped", study_round::Step::stopped)
        .value("invalid", study_round::Step::invalid);

    using study_python::Session;
    // Keep the GIL throughout each call. No native state is shared by clones.
    py::class_<Session>(m, "Session")
        .def(py::init<std::uint64_t, int>(), py::arg("seed").noconvert(),
             py::arg("gravity_interval").noconvert() = 30)
        .def("reset", &Session::reset, py::arg("seed").noconvert(),
             py::arg("gravity_interval").noconvert() = 30)
        .def("clone", &Session::clone)
        .def("step", &Session::step, py::arg("mask").noconvert())
        .def("snapshot", [](const Session& session) {
            const auto snapshot = session.snapshot();
            py::dict result;
            result["board"] = snapshot.board;
            result["current_id"] = snapshot.current_id;
            result["next_id"] = snapshot.next_id;
            return result;
        })
        .def("legal_actions", &Session::legal_actions)
        .def("action_trace", &Session::action_trace, py::arg("action").noconvert())
        .def("apply_action", [](Session& session,int action) {
            const auto applied=session.apply_action(action);
            py::dict result;
            result["ticks"]=applied.ticks;
            result["lines"]=applied.lines;
            result["ended"]=applied.ended;
            return result;
        }, py::arg("action").noconvert())
        .def("finished", &Session::finished)
        .def("score", &Session::score)
        .def("grid", &Session::grid, "Independent nested lists of locked cells.")
        .def("state_bytes", [](const Session& session) {
            const auto data = session.state_bytes();
            return py::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        }, "A copied canonical state, for comparison rather than restore.");

    m.def("observation_schema", [] {
        py::dict schema;
        schema["version"] = 1;
        schema["rows"] = study_grid::Grid::kRows;
        schema["cols"] = study_grid::Grid::kColumns;
        schema["piece_ids"] = Session::piece_ids();
        return schema;
    });
    m.def("action_schema", [] {
        py::dict schema;
        schema["version"]=1;
        schema["columns"]=study_actions::kColumns;
        schema["orientations"]=study_actions::kOrientations;
        schema["count"]=study_actions::kCount;
        return schema;
    });
    m.attr("ROWS") = study_grid::Grid::kRows;
    m.attr("COLS") = study_grid::Grid::kColumns;
    m.attr("LEFT") = static_cast<unsigned>(study_input::left);
    m.attr("RIGHT") = static_cast<unsigned>(study_input::right);
    m.attr("DOWN") = static_cast<unsigned>(study_input::down);
    m.attr("ROTATE") = static_cast<unsigned>(study_input::rotate);
    m.attr("DROP") = static_cast<unsigned>(study_input::drop);
    m.attr("KNOWN") = static_cast<unsigned>(study_input::known);
}
