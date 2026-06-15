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

function(add_host_test target_name)
    cmake_parse_arguments(ARG "" "" "SOURCES;INCLUDE_DIRS" ${ARGN})

    add_executable(${target_name}
        ${ARG_SOURCES}
    )

    target_include_directories(${target_name} PRIVATE
        ${ARG_INCLUDE_DIRS}
    )

    set_target_properties(${target_name} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
    )

    target_link_libraries(${target_name} PRIVATE unity)

    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_host_test(sl_go_mini_common_unit_tests
    SOURCES
        main/common_unit_tests_main.cpp
        ../components/common/test/settings_unit_test.cpp
        ../components/common/test/utils_unit_test.cpp
        ../components/common/utils.cpp
    INCLUDE_DIRS
        ../components/common
)

add_host_test(sl_go_mini_system_manager_host_integration_tests
    SOURCES
        main/system_manager_host_integration_tests_main.cpp
        ../components/system_manager/system_manager.cpp
        ../components/system_manager/test/system_manager_host_integration_test.cpp
    INCLUDE_DIRS
        support
        ../components/common
        ../components/system_manager
        ../components/button_driver
)
