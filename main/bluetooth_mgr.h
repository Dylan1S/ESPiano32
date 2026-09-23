/**
 * BLE MIDI central scans for, connects to and subscribes to ble-midi peripheral.
 * Each parsed MIDI message is handed to the caller.
 * Reconnections happen automatically after disconnecting
 * 
 */

#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * Callback prototype for MIDI messages. runs on default esp event loop task on ble receive path.
 * Must be nonblocking
 */
typedef void (*bluetooth_mgr_midi_cb_t)(uint16_t timestamp_ms, const uint8_t *msg, uint16_t msg_len);

/**
 * Start the BLE stack and begin scanning for the piano.
 *
 * Requires NVS and the default event loop to be initialised first.
 */
esp_err_t bluetooth_mgr_start(bluetooth_mgr_midi_cb_t on_midi);
