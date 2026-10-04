cmake_minimum_required(VERSION 3.18)

project(study_roles LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

if(MSVC)
  add_compile_options(/utf-8)
endif()

get_filename_component(STUDY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

set(STUDY_ROLE "RULES" CACHE STRING "Which implemented role material to build")
set_property(CACHE STUDY_ROLE PROPERTY STRINGS RULES SERVICE TRAINING POLICY STORAGE SHUTDOWN)

if(NOT STUDY_ROLE MATCHES "^(RULES|SERVICE|TRAINING|POLICY|STORAGE|SHUTDOWN)$")
  message(FATAL_ERROR "Unknown STUDY_ROLE '${STUDY_ROLE}': expected RULES, SERVICE, TRAINING, POLICY, STORAGE, or SHUTDOWN.")
endif()

if(STUDY_ROLE STREQUAL "RULES")
  add_library(study_rules INTERFACE)
  target_include_directories(study_rules INTERFACE "${STUDY_SOURCE}")
  target_compile_features(study_rules INTERFACE cxx_std_17)

  add_executable(binding_oracle "${STUDY_SOURCE}/tools/binding_oracle.cpp")
  target_link_libraries(binding_oracle PRIVATE study_rules)

  add_executable(policy_match_contract "${STUDY_SOURCE}/tests/policy_match_contract.cpp")
  target_link_libraries(policy_match_contract PRIVATE study_rules)

  add_test(NAME binding_oracle COMMAND binding_oracle 77 48 0 16)
  add_test(NAME policy_match_contract COMMAND policy_match_contract)

elseif(STUDY_ROLE STREQUAL "SERVICE")
  enable_language(C)

  find_package(Threads REQUIRED)
  find_package(OpenSSL REQUIRED COMPONENTS Crypto)

  set(STUDY_VENDOR_DIR "${STUDY_SOURCE}/../../../../third_party" CACHE PATH "Shared vendored third-party sources")
  get_filename_component(STUDY_VENDOR_DIR "${STUDY_VENDOR_DIR}" ABSOLUTE)

  foreach(_vendor_file httplib.h json.hpp sqlite3.c sqlite3.h)
    if(NOT EXISTS "${STUDY_VENDOR_DIR}/${_vendor_file}")
      message(FATAL_ERROR "Missing vendored file: ${STUDY_VENDOR_DIR}/${_vendor_file}")
    endif()
  endforeach()

  add_library(study_sqlite STATIC "${STUDY_VENDOR_DIR}/sqlite3.c")
  set_target_properties(study_sqlite PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO)
  target_include_directories(study_sqlite SYSTEM PUBLIC "${STUDY_VENDOR_DIR}")
  target_link_libraries(study_sqlite PUBLIC Threads::Threads ${CMAKE_DL_LIBS})

  add_library(study_accounts INTERFACE)
  target_compile_features(study_accounts INTERFACE cxx_std_17)
  target_include_directories(study_accounts INTERFACE "${STUDY_SOURCE}")
  target_include_directories(study_accounts SYSTEM INTERFACE "${STUDY_VENDOR_DIR}")
  target_link_libraries(study_accounts INTERFACE study_sqlite OpenSSL::Crypto Threads::Threads)
  if(WIN32)
    target_link_libraries(study_accounts INTERFACE ws2_32 crypt32)
  endif()

  add_executable(study_account_db "${STUDY_SOURCE}/meta/account_service.cpp"
    "${STUDY_SOURCE}/platform/utf8_arguments.cpp")
  target_link_libraries(study_account_db PRIVATE study_accounts)
  if(MINGW)
    target_link_options(study_account_db PRIVATE -municode)
  endif()
  if(WIN32)
    add_executable(utf8_arguments_test "${STUDY_SOURCE}/tests/utf8_arguments_test.cpp"
      "${STUDY_SOURCE}/platform/utf8_arguments.cpp")
    target_include_directories(utf8_arguments_test PRIVATE "${STUDY_SOURCE}")
    target_compile_features(utf8_arguments_test PRIVATE cxx_std_17)
    add_test(NAME utf8_arguments COMMAND utf8_arguments_test)
  endif()

  # account_contract requires a fresh caller-owned DB path, so the audit
  # builds it here but invokes it directly rather than registering CTest.
  add_executable(account_contract "${STUDY_SOURCE}/tests/account_contract.cpp")
  target_link_libraries(account_contract PRIVATE study_accounts)

elseif(STUDY_ROLE STREQUAL "SHUTDOWN")
  find_package(Threads REQUIRED)
  add_executable(shutdown_demo
    "${STUDY_SOURCE}/tools/shutdown_demo.cpp"
    "${STUDY_SOURCE}/net/socket.cpp"
    "${STUDY_SOURCE}/net/stream.cpp"
    "${STUDY_SOURCE}/net/receive_socket.cpp"
    "${STUDY_SOURCE}/net/send_socket.cpp"
    "${STUDY_SOURCE}/net/thread_link.cpp")
  target_include_directories(shutdown_demo PRIVATE "${STUDY_SOURCE}")
  target_compile_features(shutdown_demo PRIVATE cxx_std_17)
  target_link_libraries(shutdown_demo PRIVATE Threads::Threads)
  if(WIN32)
    target_link_libraries(shutdown_demo PRIVATE ws2_32)
  endif()
  add_executable(stop_gate_contract "${STUDY_SOURCE}/tests/stop_gate_contract.cpp")
  target_include_directories(stop_gate_contract PRIVATE "${STUDY_SOURCE}")
  target_compile_features(stop_gate_contract PRIVATE cxx_std_17)
  target_link_libraries(stop_gate_contract PRIVATE Threads::Threads)
  add_test(NAME stop_gate_contract COMMAND stop_gate_contract)
  set_tests_properties(stop_gate_contract PROPERTIES TIMEOUT 10)
  add_test(NAME shutdown_self_stop COMMAND shutdown_demo --self-stop)
  set_tests_properties(shutdown_self_stop PROPERTIES TIMEOUT 10)

elseif(STUDY_ROLE STREQUAL "STORAGE")
  add_executable(private_publish
    "${STUDY_SOURCE}/tools/private_publish.cpp"
    "${STUDY_SOURCE}/settings/private_file.cpp"
    "${STUDY_SOURCE}/platform/utf8_arguments.cpp")
  target_include_directories(private_publish PRIVATE "${STUDY_SOURCE}")
  target_compile_features(private_publish PRIVATE cxx_std_17)
  if(WIN32)
    target_link_libraries(private_publish PRIVATE advapi32)
  endif()
  if(MINGW)
    target_link_options(private_publish PRIVATE -municode)
  endif()

elseif(STUDY_ROLE STREQUAL "TRAINING")
  add_subdirectory("${STUDY_SOURCE}/bindings" "${CMAKE_CURRENT_BINARY_DIR}/bindings")

elseif(STUDY_ROLE STREQUAL "POLICY")
  add_subdirectory("${STUDY_SOURCE}/inference" "${CMAKE_CURRENT_BINARY_DIR}/inference")
endif()
