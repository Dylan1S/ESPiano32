/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * BLE MIDI central, derived from Espressif's ble_midi_central example.
 *
 * Scans for a BLE-MIDI peripheral (service UUID and/or CONFIG_EXAMPLE_PEER_NAME),
 * connects, discovers GATT, pairs, enables notifications on the MIDI I/O
 * characteristic, and forwards parsed MIDI messages to the registered callback.
 */

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"

#include "esp_ble_conn_mgr.h"
#include "esp_ble_midi.h"
#include "esp_ble_midi_svc.h"

#include "bluetooth_mgr.h"

static const char *TAG = "bluetooth_mgr";

static const uint8_t s_midi_svc_uuid[] = BLE_MIDI_SERVICE_UUID128;
static const uint8_t s_midi_char_uuid[] = BLE_MIDI_CHAR_UUID128;

static bool s_target_found;
static uint16_t s_conn_handle = BLE_CONN_HANDLE_INVALID;
static bluetooth_mgr_midi_cb_t s_on_midi;

static bool uuid128_adv_match(const uint8_t *adv, uint8_t adv_len)
{
    const uint8_t *p = NULL;
    uint8_t plen = 0;

    if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_UUID128_COMPLETE, &p, &plen) == ESP_OK) {
        if (plen == 16 && memcmp(p, s_midi_svc_uuid, 16) == 0) {
            return true;
        }
    }
    if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_UUID128_INCOMP, &p, &plen) == ESP_OK) {
        if (plen == 16 && memcmp(p, s_midi_svc_uuid, 16) == 0) {
            return true;
        }
    }
    return false;
}

static bool peer_name_match(const uint8_t *adv, uint8_t adv_len)
{
    const char *want = CONFIG_EXAMPLE_PEER_NAME;
    if (want[0] == '\0') {
        return false;
    }

    const uint8_t *name_ptr = NULL;
    uint8_t name_len = 0;
    size_t want_len = strlen(want);

    if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_NAME_COMPLETE, &name_ptr, &name_len) != ESP_OK) {
        if (esp_ble_conn_parse_adv_data(adv, adv_len, ESP_BLE_CONN_ADV_TYPE_NAME_SHORT, &name_ptr, &name_len) != ESP_OK) {
            return false;
        }
    }

    if (name_len != want_len) {
        return false;
    }
    return (memcmp(name_ptr, want, want_len) == 0);
}

static bool scan_cb(const esp_ble_conn_scan_result_t *result, void *arg)
{
    (void)arg;
    if (s_target_found || !result) {
        return false;
    }

    ESP_LOGD(TAG, "scan: addr=" BLE_CONN_MGR_ADDR_STR " rssi=%d adv_len=%u",
             BLE_CONN_MGR_ADDR_HEX(result->addr), result->rssi, result->adv_data_len);

    if (uuid128_adv_match(result->adv_data, result->adv_data_len) || peer_name_match(result->adv_data, result->adv_data_len)) {
        ESP_LOGI(TAG, "Matched BLE-MIDI peripheral, stopping scan");
        s_target_found = true;
        esp_ble_conn_scan_stop();
        return true;
    }

    return false;
}

static void midi_rx_cb(const uint8_t *data, uint16_t len, void *user_ctx)
{
    (void)user_ctx;
    ESP_LOGI(TAG, "MIDI RX BEP (%u bytes):", len);
    ESP_LOG_BUFFER_HEX(TAG, data, len);
}

static void midi_evt_cb(uint16_t ts_ms, esp_ble_midi_event_type_t event_type, const uint8_t *msg, uint16_t msg_len)
{
    if (event_type == ESP_BLE_MIDI_EVENT_SYSEX_OVERFLOW) {
        ESP_LOGW(TAG, "MIDI EVT ts=%u: SysEx buffer overflow", ts_ms);
        return;
    }
    if (msg == NULL || msg_len == 0 || s_on_midi == NULL) {
        return;
    }
    s_on_midi(ts_ms, msg, msg_len);
}

static bool conn_data_is_midi_io(const esp_ble_conn_data_t *d)
{
    if (!d || d->type != BLE_CONN_UUID_TYPE_128) {
        return false;
    }
    return memcmp(d->uuid.uuid128, s_midi_char_uuid, sizeof(d->uuid.uuid128)) == 0;
}

static void subscribe_midi_notifications(uint16_t conn_handle)
{
    static uint8_t cccd_notify[2] = {0x01, 0x00};

    esp_ble_conn_data_t sub = {
        .type = BLE_CONN_UUID_TYPE_128,
        .write_conn_id = conn_handle,
        .data = cccd_notify,
        .data_len = sizeof(cccd_notify),
    };
    memcpy(sub.uuid.uuid128, s_midi_char_uuid, sizeof(sub.uuid.uuid128));

    esp_err_t err = esp_ble_conn_subscribe_by_handle(conn_handle, ESP_BLE_CONN_DESC_CIENT_CONFIG, &sub);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Subscribe MIDI notify failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Subscribed to MIDI I/O notifications");
    }
}

static void conn_event_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)handler_args;
    if (base != BLE_CONN_MGR_EVENTS) {
        return;
    }

    switch (id) {
    case ESP_BLE_CONN_EVENT_CONNECTED:
        if (esp_ble_conn_get_conn_handle(&s_conn_handle) == ESP_OK) {
            if (s_conn_handle == BLE_CONN_HANDLE_INVALID) {
                ESP_LOGW(TAG, "Connected event but connection handle is invalid");
            } else {
                ESP_LOGI(TAG, "Connected, conn_handle=%u", s_conn_handle);
                esp_err_t mtu_rc = esp_ble_conn_mtu_update(s_conn_handle, 185);
                if (mtu_rc != ESP_OK) {
                    ESP_LOGW(TAG, "MTU exchange failed: %s", esp_err_to_name(mtu_rc));
                }
            }
        }
        break;

    case ESP_BLE_CONN_EVENT_DISC_COMPLETE:
        ESP_LOGI(TAG, "GATT discovery complete");
        if (s_conn_handle == BLE_CONN_HANDLE_INVALID) {
            break;
        }
        if (esp_ble_conn_security_initiate(s_conn_handle) == ESP_OK) {
            ESP_LOGI(TAG, "Pairing with peer");
        } else {
            ESP_LOGW(TAG, "Could not start pairing; subscribing unencrypted");
            subscribe_midi_notifications(s_conn_handle);
        }
        break;

    case ESP_BLE_CONN_EVENT_ENC_CHANGE: {
        esp_ble_conn_event_data_t *evt = (esp_ble_conn_event_data_t *)event_data;
        if (!evt || evt->enc_change.conn_handle != s_conn_handle) {
            break;
        }
        if (evt->enc_change.status == 0 && evt->enc_change.encrypted) {
            ESP_LOGI(TAG, "Link encrypted");
        } else {
            ESP_LOGW(TAG, "Pairing failed (status=%d); subscribing anyway", evt->enc_change.status);
        }
        subscribe_midi_notifications(s_conn_handle);
        break;
    }

    case ESP_BLE_CONN_EVENT_DATA_RECEIVE: {
        esp_ble_conn_data_t *conn_data = (esp_ble_conn_data_t *)event_data;
        if (!conn_data || !conn_data->data || conn_data->data_len == 0) {
            break;
        }
        if (conn_data_is_midi_io(conn_data)) {
            esp_ble_midi_on_bep_received(conn_data->data, conn_data->data_len);
        } else {
            ESP_LOGW(TAG, "DATA_RECEIVE for non-MIDI UUID, len=%u", conn_data->data_len);
        }
        break;
    }

    case ESP_BLE_CONN_EVENT_MTU: {
        esp_ble_conn_event_data_t *evt = (esp_ble_conn_event_data_t *)event_data;
        if (evt) {
            ESP_LOGI(TAG, "MTU updated: effective=%u", evt->mtu_update.mtu);
        }
        break;
    }

    case ESP_BLE_CONN_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "Disconnected");
        s_conn_handle = BLE_CONN_HANDLE_INVALID;
        s_target_found = false;
        if (esp_ble_conn_scan_start() != ESP_OK) {
            ESP_LOGW(TAG, "Failed to restart scan");
        }
        break;

    default:
        break;
    }
}

esp_err_t bluetooth_mgr_start(bluetooth_mgr_midi_cb_t on_midi)
{
    s_on_midi = on_midi;

    esp_ble_conn_config_t config = {0};
    strncpy((char *)config.device_name, CONFIG_EXAMPLE_LOCAL_NAME, sizeof(config.device_name) - 1);
    strncpy((char *)config.broadcast_data, CONFIG_EXAMPLE_BLE_SUB_ADV, sizeof(config.broadcast_data) - 1);

    ESP_RETURN_ON_ERROR(esp_event_handler_register(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, conn_event_handler, NULL),
                        TAG, "register event handler");

    ESP_RETURN_ON_ERROR(esp_ble_conn_init(&config), TAG, "conn mgr init");
    ESP_RETURN_ON_ERROR(esp_ble_conn_register_scan_callback(scan_cb, NULL), TAG, "register scan callback");

    ESP_RETURN_ON_ERROR(esp_ble_midi_svc_init(), TAG, "MIDI service init");
    ESP_RETURN_ON_ERROR(esp_ble_midi_profile_init(), TAG, "MIDI profile init");
    ESP_RETURN_ON_ERROR(esp_ble_midi_register_rx_cb(midi_rx_cb, NULL), TAG, "register MIDI rx callback");
    ESP_RETURN_ON_ERROR(esp_ble_midi_register_event_cb(midi_evt_cb), TAG, "register MIDI event callback");

    esp_err_t ret = esp_ble_conn_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE: %s", esp_err_to_name(ret));
        esp_ble_midi_profile_deinit();
        esp_ble_midi_svc_deinit();
        esp_ble_conn_deinit();
        esp_event_handler_unregister(BLE_CONN_MGR_EVENTS, ESP_EVENT_ANY_ID, conn_event_handler);
        return ret;
    }

    ESP_LOGI(TAG, "Central started; scanning for BLE-MIDI peripheral (UUID%s%s)",
             CONFIG_EXAMPLE_PEER_NAME[0] ? "; name match: " : "",
             CONFIG_EXAMPLE_PEER_NAME[0] ? CONFIG_EXAMPLE_PEER_NAME : "");
    return ESP_OK;
}
