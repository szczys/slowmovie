#include <Arduino.h>

#include "credentials.h"
#include "console.h"
#include "epaper.hpp"
#include "golioth.h"
#include "wifi.h"

#define TAG "main"

extern "C" void app_main()
{
    initArduino();

    console_init();

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
