#include <SPI.h>

#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include "elf.h"

#include <esp_log.h>
#define TAG "epaper"

/* Pinout
 * Epaper | ESP32
 * -------|-----------------
 * 3v3    | 3v3
 * Gnd    | Gnd
 * DC     | 0     15  5
 * Busy   | 15    32  6
 * MOSI   | 23    19  11
 * Clk    | 18    5   12
 * CS     | 5     33  10
 * Rst    | 2     14  7
 */
//GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> epd(GxEPD2_750c_Z08(33, 15, 14, 32)); // GDEW075Z08 800x480, GD7965
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT / 4> epd(GxEPD2_750c_Z08(10, 5, 7, 6)); // GDEW075Z08 800x480, GD7965

static void update_display(const uint8_t *buffer_black, bool inverted) {
  epd.firstPage();
  do
  {
    if (inverted) {
      epd.drawInvertedBitmap(0, 0, buffer_black, epd.epd2.WIDTH, epd.epd2.HEIGHT, GxEPD_BLACK);
    } else {
      epd.drawBitmap(0, 0, buffer_black, epd.epd2.WIDTH, epd.epd2.HEIGHT, GxEPD_BLACK);
    }
  }
  while (epd.nextPage());

  epd.hibernate();
}

void update_from_buffer(const uint8_t *buf, size_t buf_len)
{
  size_t display_bytes = (epd.epd2.WIDTH * epd.epd2.HEIGHT) / 8;

  if (display_bytes > buf_len)
  {
    ESP_LOGE(TAG, "Buffer too small: %zu > %zu", display_bytes, buf_len);
    return;
  }

  size_t data_offset = buf_len - display_bytes;
  ESP_LOGI(TAG, "Buffer offset: %zu", data_offset);

  update_display(buf + data_offset, true);
}

void epaper_init()
{
    epd.init(115200, true, 50, false);
}

void epaper_show_splash(void)
{
  update_display((uint8_t *) elf, false);
}
