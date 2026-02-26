idf_component_register(
    SRCS "wifi_interface.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_wifi esp_event esp_netif nvs_flash common
)