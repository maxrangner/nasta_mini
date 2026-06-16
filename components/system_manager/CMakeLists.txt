idf_component_register(
    SRCS "system_manager.cpp"
    INCLUDE_DIRS "."
    REQUIRES common wifi_interface button_driver display
)
