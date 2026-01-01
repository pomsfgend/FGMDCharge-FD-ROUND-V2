# FD_ROUND V2 application component.
# LCD driver is vendored for deterministic ESP-IDF 5.0.9 builds.
# Web UI assets are compiled into static C arrays; no linker EMBED_* symbols are used.
idf_component_register(
    SRCS
        "main.c"
        "app_state.c"
        "can_gbt.c"
        "gbt_tp.c"
        "display.c"
        "lv_font_ru_14.c"
        "touch_cst816s.c"
        "ble.c"
        "web.c"
        "web_assets.c"
        "safe_ota.c"
        "lzss.c"
        "settings.c"
        "adc_pwm.c"
        "sm_relay.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_lcd
        esp_lcd_ili9341
        esp_http_server
        esp_wifi
        esp_event
        esp_netif
        nvs_flash
        bt
        app_update
        spi_flash
        esp_timer
        esp_http_client
        lvgl
)
