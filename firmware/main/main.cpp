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

extern "C" void app_main()
{
    initArduino();

    struct slowmovie_creds creds;
    int err = cred_load_all(&creds);
    if (0 != err)
    {
        ESP_LOGE(TAG, "Failed to load credentials.");
        return;
    }

    start_wifi(&creds.wifi_ssid, &creds.wifi_psk);

    ESP_LOGI(TAG, "Slowmovie start.");

    epaper_init();
    epaper_show_splash();

    golioth_register_frames(&creds, update_from_buffer);
}
