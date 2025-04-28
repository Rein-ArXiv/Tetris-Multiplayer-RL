// This is Simple C++ Env File to test pybind. Not related with this github project.


#include <random>
#include <tuple>
#include <pybind11/pybind11.h>

namespace py = pybind11;

class SimpleEnv{
public:
    SimpleEnv(){
        reset();
    }

    int reset(){
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, 9);
        target = distrib(gen);
        step_count = 0;
        return 0;
    }

    std::tuple<int, double, bool> step(int action){
        step_count++;
        bool done = (action == target);
        double reward = done ? 1.0 : -0.1;
        
        if (step_count >= 100) done = true; // End at 100 turns.

        return std::make_tuple(step_count, reward, done);
    }

private:
    int target = 0;
    int step_count = 0;
};

PYBIND11_MODULE(simple_env, m){
    py::class_<SimpleEnv>(m, "SimpleEnv")
        .def(py::init<>())
        .def("reset", &SimpleEnv::reset)
        .def("step", &SimpleEnv::step);
}

