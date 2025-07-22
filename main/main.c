/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "gap.h"
#include "gatt_svc.h"

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);
static void weight_task(void *param);

/* Private functions */
static void on_stack_reset(int reason) {
    ESP_LOGI(TAG, "nimble stack reset, reason: %d", reason);
}

static void on_stack_sync(void) {
    adv_init();  // Start advertising
}

static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "nimble host task started!");
    nimble_port_run(); // Blocks until nimble_port_stop() is called
    vTaskDelete(NULL);
}

/* Task: update and send weight */
static void weight_task(void *param) {
    ESP_LOGI(TAG, "weight task started!");

    while (1) {
        send_weight_indication();
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second
    }

    vTaskDelete(NULL);
}

/* Application entry point */
void app_main(void) {
    int rc;
    esp_err_t ret;

    // Initialize NVS flash (BLE stack uses it to store configurations)
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error: %d", ret);
        return;
    }

    // Initialize NimBLE stack
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error: %d", ret);
        return;
    }

    // Initialize GAP (advertising setup)
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error: %d", rc);
        return;
    }

    // Initialize GATT server (weight service)
    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error: %d", rc);
        return;
    }

    // Configure NimBLE host
    nimble_host_config_init();

    // Start NimBLE host task
    xTaskCreate(nimble_host_task, "NimBLE Host", 4 * 1024, NULL, 5, NULL);

    // Start weight task
    xTaskCreate(weight_task, "Weight Task", 2 * 1024, NULL, 5, NULL);
}
