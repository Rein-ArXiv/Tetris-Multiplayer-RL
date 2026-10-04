cmake_minimum_required(VERSION 3.18)
# This directory can also be configured alone, without graphics/server SDKs.
project(study_python_boundary LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(PYBIND11_FINDPYTHON ON)
find_package(Python 3.10 COMPONENTS Interpreter Development.Module REQUIRED)
find_package(pybind11 2.11 CONFIG REQUIRED)

pybind11_add_module(study_py study_py.cpp)
target_include_directories(study_py PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(study_py PRIVATE cxx_std_17)
# Use one import directory for single- and multi-configuration generators.
set_target_properties(study_py PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/python/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/python/$<CONFIG>")
add_executable(binding_oracle ../tools/binding_oracle.cpp)
target_include_directories(binding_oracle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(binding_oracle PRIVATE cxx_std_17)
enable_testing()
add_test(NAME python_binding_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/binding_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>" --oracle "$<TARGET_FILE:binding_oracle>")
set_tests_properties(python_binding_contract PROPERTIES TIMEOUT 30)

# NumPy and CPU PyTorch are test dependencies for the actual tensor contract.
add_test(NAME observation_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/observation_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(observation_contract PROPERTIES TIMEOUT 60)

add_executable(action_contract ../tests/action_contract.cpp)
target_include_directories(action_contract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(action_contract PRIVATE cxx_std_17)
add_test(NAME action_contract COMMAND action_contract)
add_test(NAME action_binding COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/action_binding.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(action_contract action_binding PROPERTIES TIMEOUT 90)

add_test(NAME reward_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/reward_contract.py")
add_test(NAME reward_binding COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/reward_binding.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(reward_contract reward_binding PROPERTIES TIMEOUT 60)

add_test(NAME gym_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/gym_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(gym_contract PROPERTIES TIMEOUT 90)

add_test(NAME versus_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/versus_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(versus_contract PROPERTIES TIMEOUT 120)

add_executable(versus_session ../tests/versus_session.cpp)
target_include_directories(versus_session PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(versus_session PRIVATE cxx_std_17)
add_test(NAME versus_session COMMAND versus_session)

add_test(NAME heuristic_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/heuristic_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(heuristic_contract PROPERTIES TIMEOUT 120)

add_test(NAME policy_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/policy_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(policy_contract PROPERTIES TIMEOUT 90)

add_test(NAME ppo_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/ppo_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(ppo_contract PROPERTIES TIMEOUT 120)

add_test(NAME checkpoint_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/checkpoint_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(checkpoint_contract PROPERTIES TIMEOUT 120)

add_test(NAME experiment_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/experiment_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(experiment_contract PROPERTIES TIMEOUT 120)

add_test(NAME model_zoo_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/model_zoo_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(model_zoo_contract PROPERTIES TIMEOUT 120)

add_test(NAME onnx_export_contract COMMAND "${Python_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../tests/onnx_export_contract.py"
    --module-dir "$<TARGET_FILE_DIR:study_py>")
set_tests_properties(onnx_export_contract PROPERTIES TIMEOUT 120)

add_executable(route_contract ../tests/route_contract.cpp)
target_include_directories(route_contract PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(route_contract PRIVATE cxx_std_17)
add_test(NAME route_contract COMMAND route_contract)
set_tests_properties(route_contract PROPERTIES TIMEOUT 90)
add_executable(route_demo ../tools/route_demo.cpp)
target_include_directories(route_demo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(route_demo PRIVATE cxx_std_17)

foreach(target pacing_gate paced_contract paced_demo)
    if(target STREQUAL "paced_demo")
        add_executable(${target} ../tools/${target}.cpp)
    else()
        add_executable(${target} ../tests/${target}.cpp)
    endif()
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
    target_compile_features(${target} PRIVATE cxx_std_17)
endforeach()
add_test(NAME pacing_gate COMMAND pacing_gate)
add_test(NAME paced_contract COMMAND paced_contract)
set_tests_properties(pacing_gate paced_contract PROPERTIES TIMEOUT 90)
