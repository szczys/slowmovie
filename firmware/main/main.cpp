#include <Arduino.h>

#include "credentials.h"
#include "epaper.hpp"
#include "golioth.h"
#include "wifi.h"

#define TAG "main"

/*
 * Create a file: main/credentials.h
 *
 * Contents:
 * #define EXAMPLE_ESP_WIFI_SSID "wifi-ssid"
 * #define EXAMPLE_ESP_WIFI_PASS "wifi-password"
 * #define GOLIOTH_PSK_ID "your-psk-id"
 * #define GOLIOTH_PSK_ID "your-psk"
 */

extern "C" void app_main() {
    initArduino();

    ESP_LOGI(TAG, "Slowmovie start.");

    epaper_init();
    epaper_show_splash();

    start_wifi();

    golioth_register_frames(update_from_buffer);

    int counter = 0;

    while (true)
    {
        ESP_LOGI(TAG, "Sending hello! %d", counter);

        ++counter;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
