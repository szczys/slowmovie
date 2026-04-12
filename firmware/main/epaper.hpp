#pragma once

#include <stdint.h>
#include <stddef.h>

void epaper_init();
void epaper_show_splash(void);
void update_from_buffer(const uint8_t *buf, size_t buf_len);
