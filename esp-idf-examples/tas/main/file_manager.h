#pragma once
#include <esp_err.h>
#include <stdbool.h>
#include "wear_levelling.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 8MB FAT 파티션을 /fatfs 경로에 마운트합니다.
 */
esp_err_t init_file_system();
esp_err_t mount_file_system();
void unmount_file_system();
bool is_fs_mounted();

/**
 * @brief USB MSC에서 사용할 WL 핸들을 반환합니다.
 */
wl_handle_t get_wl_handle();

#ifdef __cplusplus
}
#endif
