#include "file_manager.h"
#include "esp_vfs_fat.h"
#include "esp_vfs.h"
#include "wear_levelling.h"
#include "esp_log.h"

static const char *TAG = "FILE_MGR";
static wl_handle_t global_wl_handle = WL_INVALID_HANDLE;
static bool fs_mounted = false;

esp_err_t init_file_system() {
    // 1. Wear Levelling 핸들을 독립적으로 먼저 초기화 (한 번만 수행)
    if (global_wl_handle == WL_INVALID_HANDLE) {
        const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "fatfs");
        if (!partition) {
            ESP_LOGE(TAG, "Failed to find fatfs partition");
            return ESP_ERR_NOT_FOUND;
        }

        esp_err_t err = wl_mount(partition, &global_wl_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to mount wear levelling (%s)", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Wear Levelling handle initialized: %d", global_wl_handle);
    }

    // 2. 초기 부팅 시 파일 시스템 마운트
    return mount_file_system();
}

esp_err_t mount_file_system() {
    if (fs_mounted) return ESP_OK;
    if (global_wl_handle == WL_INVALID_HANDLE) return ESP_ERR_INVALID_STATE;

    ESP_LOGI(TAG, "Mounting FATFS to VFS...");
    
    // wl_handle을 파괴하지 않는 esp_vfs_fat_register를 사용하거나, 
    // 기존 spiflash_mount를 쓰되 언마운트 시 핸들을 지키는 전략을 사용합니다.
    // 여기서는 표준 함수를 쓰되 언마운트 방식을 바꿉니다.
    const esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = true,
        .max_files = 5,
        .allocation_unit_size = 4096 
    };

    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl("/fatfs", "fatfs", &mount_config, &global_wl_handle);
    if (err == ESP_OK) {
        fs_mounted = true;
        ESP_LOGI(TAG, "FATFS mounted at /fatfs");
    }
    return err;
}

void unmount_file_system() {
    if (!fs_mounted) return;
    
    // 주의: esp_vfs_fat_spiflash_unmount_rw_wl은 wl_unmount를 호출하여 핸들을 파괴함.
    // 따라서 VFS만 해제하기 위해 esp_vfs_unregister를 직접 호출하거나 전략을 바꿉니다.
    // 여기서는 TinyUSB와의 공존을 위해 VFS 레이어만 언마운트하는 방식을 시도합니다.
    esp_err_t err = esp_vfs_unregister("/fatfs");
    if (err == ESP_OK) {
        fs_mounted = false;
        ESP_LOGI(TAG, "FATFS VFS unmounted (WL handle preserved)");
    } else {
        ESP_LOGE(TAG, "Failed to unregister FATFS VFS: %s", esp_err_to_name(err));
    }
}

bool is_fs_mounted() {
    return fs_mounted;
}

wl_handle_t get_wl_handle() {
    return global_wl_handle;
}
