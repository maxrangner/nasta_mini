idf_component_register(
    SRCS "system_init.cpp" "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES system_manager network_manager wifi_interface common
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    # -Werror
)