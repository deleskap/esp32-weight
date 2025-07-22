/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "gap.h"
#include "common.h"
#include "gatt_svc.h"

/* Private variables */
static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};

/* Private function declarations */
inline static void format_addr(char *addr_str, uint8_t addr[]);
static void print_conn_desc(struct ble_gap_conn_desc *desc);
static void start_advertising(void);
static int gap_event_handler(struct ble_gap_event *event, void *arg);

/* Format BLE address as string */
inline static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static void print_conn_desc(struct ble_gap_conn_desc *desc) {
    char addr_str[18] = {0};

    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);

    format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s",
             desc->our_id_addr.type, addr_str);

    format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s",
             desc->peer_id_addr.type, addr_str);

    ESP_LOGI(TAG,
             "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
             "encrypted=%d, authenticated=%d, bonded=%d",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
             desc->sec_state.encrypted, desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

static void start_advertising(void) {
    int rc;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    /* Set device name */
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;

    /* Add Weight Scale Service UUID (0x181D) */
    static const ble_uuid16_t svc_uuid = BLE_UUID16_INIT(0x181D);
    adv_fields.uuids16 = (ble_uuid16_t *)&svc_uuid;
    adv_fields.num_uuids16 = 1;
    adv_fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set advertising data, rc=%d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to start advertising, rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}

static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    int rc = 0;
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                print_conn_desc(&desc);
            }
        } else {
            start_advertising();
        }
        return rc;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected from peer; reason=%d",
                 event->disconnect.reason);
        start_advertising();
        return rc;

    case BLE_GAP_EVENT_SUBSCRIBE:
        gatt_svr_subscribe_cb(event);
        return rc;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        return rc;
    }
    return rc;
}

/* Public functions */
void adv_init(void) {
    int rc;
    char addr_str[18] = {0};

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "device does not have any available bt address!");
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer address type, rc=%d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to copy device address, rc=%d", rc);
        return;
    }

    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    start_advertising();
}

int gap_init(void) {
    ble_svc_gap_init();
    return ble_svc_gap_device_name_set(DEVICE_NAME);
}
