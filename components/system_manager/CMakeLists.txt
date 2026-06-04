idf_component_register(
    SRCS "system_manager.cpp" "led_matrix.cpp"
    INCLUDE_DIRS "."
    REQUIRES common wifi_interface button_driver led_strip
)
