#include "usb_msc.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"
#include "esp_check.h"
#include "esp_log.h"
#include "file_manager.h"

static const char *TAG = "USB_MSC";

static void storage_mount_changed_cb(tinyusb_msc_event_t *event) {
    if (event->type == TINYUSB_MSC_EVENT_MOUNT_CHANGED) {
        ESP_LOGI(TAG, "Storage mount changed. is_mounted: %s", event->mount_changed_data.is_mounted ? "YES" : "NO");
        if (event->mount_changed_data.is_mounted) {
            ESP_LOGI(TAG, "USB Storage mounted to PC. Unmounting local FATFS...");
            unmount_file_system();
        } else {
            ESP_LOGI(TAG, "USB Storage unmounted from PC. Re-mounting local FATFS...");
            mount_file_system();
        }
    }
}

esp_err_t init_usb_msc() {
    ESP_LOGI(TAG, "Initializing USB MSC Device...");
    
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .string_descriptor_count = 0,
        .external_phy = false, 
        .configuration_descriptor = NULL,
        .self_powered = false,
        .vbus_monitor_io = -1
    };

    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver (%s)", esp_err_to_name(err));
        return err;
    }

    const tinyusb_msc_spiflash_config_t msc_config = {
        .wl_handle = get_wl_handle(),
        .callback_mount_changed = storage_mount_changed_cb,
        .callback_premount_changed = NULL,
        .mount_config = {
            .format_if_mount_failed = true,
            .max_files = 5,
            .allocation_unit_size = 4096
        }
    };

    err = tinyusb_msc_storage_init_spiflash(&msc_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init MSC storage (%s)", esp_err_to_name(err));
        tinyusb_driver_uninstall();
        return err;
    }

    ESP_LOGI(TAG, "USB MSC initialized. Connection to PC is now available.");
    return ESP_OK;
}
