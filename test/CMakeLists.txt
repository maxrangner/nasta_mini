cmake_minimum_required(VERSION 3.16)

project(sl_go_mini_tests LANGUAGES C CXX)

enable_testing()

set(UNITY_DIR "${CMAKE_CURRENT_LIST_DIR}/../third_party/unity")
set(UNITY_SRC "${UNITY_DIR}/src/unity.c")

if(NOT EXISTS "${UNITY_SRC}")
    message(FATAL_ERROR "Unity source not found: ${UNITY_SRC}")
endif()

add_library(unity STATIC
    "${UNITY_SRC}"
)

target_include_directories(unity PUBLIC
    "${UNITY_DIR}/src"
)

add_executable(sl_go_mini_tests
    main/test_app_main.cpp
    ../components/common/test/test_main.cpp
    ../components/common/test/settings_test.cpp
    ../components/common/test/utils_test.cpp
    ../components/common/utils.cpp
)

target_include_directories(sl_go_mini_tests PRIVATE
    ../components/common
)

target_compile_features(sl_go_mini_tests PRIVATE cxx_std_17)

target_link_libraries(sl_go_mini_tests PRIVATE unity)

add_test(NAME sl_go_mini_tests COMMAND sl_go_mini_tests)
