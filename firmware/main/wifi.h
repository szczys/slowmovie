#ifdef __cplusplus
extern "C"
{
#endif

#pragma once

#include "credentials.h"

void start_wifi(struct credential *ssid, struct credential *psk);

#ifdef __cplusplus
}
#endif
