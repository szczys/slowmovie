#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef void (*golioth_frame_cb_t)(const uint8_t *buf, size_t buf_len);

void golioth_register_frames(golioth_frame_cb_t);

#ifdef __cplusplus
}
#endif
