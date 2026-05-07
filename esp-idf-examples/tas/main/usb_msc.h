#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 8MB FAT 파티션을 USB 이동식 디스크로 노출합니다.
 */
esp_err_t init_usb_msc();

#ifdef __cplusplus
}
#endif
