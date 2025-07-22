/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "gatt_svc.h"
#include "common.h"

/* Private function declarations */
static int weight_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg);
static void update_weight(void);

/* Private variables */
/* Weight Scale Service UUID (standard: 0x181D) */
static const ble_uuid16_t weight_svc_uuid = BLE_UUID16_INIT(0x181D);

/* Weight Measurement Characteristic UUID (standard: 0x2A9D) */
static uint16_t weight_chr_val_handle;
static const ble_uuid16_t weight_chr_uuid = BLE_UUID16_INIT(0x2A9D);

/* Bufor na dane: [0] Flags, [1-2] Waga w jednostkach 0.005 kg */
static uint8_t weight_chr_val[3] = {0};

/* Połączenie i status subskrypcji */
static uint16_t weight_chr_conn_handle = 0;
static bool weight_chr_conn_handle_inited = false;
static bool weight_ind_status = false;

/* Wewnętrzna logika zmiany wagi */
static float weight_value = 1.0f;   // Aktualna waga (1–10 kg)
static int weight_direction = 1;    // 1 = w górę, -1 = w dół

/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &weight_svc_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &weight_chr_uuid.u,
                    .access_cb = weight_chr_access,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                    .val_handle = &weight_chr_val_handle
                },
                {0},
            },
    },
    {0},
};

/* Aktualizacja wagi co 0.5 kg w zakresie 1–10 kg */
static void update_weight(void) {
    weight_value += 0.5f * weight_direction;

    if (weight_value >= 10.0f) {
        weight_value = 10.0f;
        weight_direction = -1;
    } else if (weight_value <= 1.0f) {
        weight_value = 1.0f;
        weight_direction = 1;
    }

    // Konwersja na jednostki 0.005 kg (np. 70.0 kg -> 70 / 0.005 = 14000)
    uint16_t weight_ble = (uint16_t)(weight_value / 0.005f);

    weight_chr_val[0] = 0x01; // Flags: 0x01 -> wartość w kilogramach
    weight_chr_val[1] = weight_ble & 0xFF;
    weight_chr_val[2] = (weight_ble >> 8) & 0xFF;

    ESP_LOGI(TAG, "Weight updated: %.1f kg (BLE Encoded: %02X %02X %02X)",
             weight_value, weight_chr_val[0], weight_chr_val[1], weight_chr_val[2]);
}

/* Characteristic access callback */
static int weight_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == weight_chr_val_handle) {
            rc = os_mbuf_append(ctxt->om, weight_chr_val, sizeof(weight_chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        return BLE_ATT_ERR_UNLIKELY;

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

/* Public functions */
void send_weight_indication(void) {
    update_weight(); // Aktualizuj wagę przed wysłaniem

    if (weight_ind_status && weight_chr_conn_handle_inited) {
        ble_gatts_indicate(weight_chr_conn_handle, weight_chr_val_handle);
        ESP_LOGI(TAG, "Weight indication sent: %.1f kg", weight_value);
    }
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    default:
        break;
    }
}

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    if (event->subscribe.attr_handle == weight_chr_val_handle) {
        weight_chr_conn_handle = event->subscribe.conn_handle;
        weight_chr_conn_handle_inited = true;
        weight_ind_status = event->subscribe.cur_indicate;
        ESP_LOGI(TAG, "Subscribe event for weight characteristic");
    }
}

int gatt_svc_init(void) {
    int rc;
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    return rc;
}
