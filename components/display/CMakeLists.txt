idf_component_register(
    SRCS "display.cpp" "led_matrix.cpp"
    INCLUDE_DIRS "."
    REQUIRES common led_strip driver
)
