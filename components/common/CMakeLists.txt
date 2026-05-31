idf_component_register(
    SRCS "utils.cpp" "settings_storage.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash
)
