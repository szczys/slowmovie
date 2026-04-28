#pragma once

#include <stddef.h>
#include <stdint.h>

int decompress(const uint8_t *src, size_t src_len, uint8_t *dst, size_t *dst_len);
