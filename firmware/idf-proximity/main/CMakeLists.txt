idf_component_register(SRCS "app_main.cpp" "../../led/src/NearGlow.cpp"
    INCLUDE_DIRS "." "../../led/include"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_driver_rmt esp_driver_usb_serial_jtag esp_timer mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
