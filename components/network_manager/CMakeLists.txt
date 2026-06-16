idf_component_register(
    SRCS "network_manager.cpp" "setup_portal.cpp"
    INCLUDE_DIRS "."
    REQUIRES common wifi_interface esp_http_client esp_http_server esp-tls cjson esp_netif lwip
)
