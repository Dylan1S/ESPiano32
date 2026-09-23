# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

ESP32-S3 firmware that connects to a Roland FP-30X digital piano over BLE
MIDI and reports what's being played. Built with ESP-IDF (not Arduino),
starting from Espressif's `ble_midi_central` component-registry example
(`espressif/ble_midi`, which depends on `ble_conn_mgr` for the underlying
NimBLE scan/connect/GATT work).

## Current milestone — this is the only scope right now

Show the piano keys currently held down, printed over the USB serial
connection to this computer (`idf.py monitor` / `ESP_LOGI`), as the pianist
plays. No display hardware is involved yet.

Concretely: the ESP32-S3 scans for and connects to the FP-30X as a BLE MIDI
central, receives Note On/Off messages, and prints the currently-held notes
to serial whenever the set changes.

## Next milestone — not started yet

Once key display works, extend it to identify and print the chord being
played (e.g. "Cmaj7") instead of, or alongside, the raw note list — still
over serial, no new hardware.

## Out of scope for now

Do not add or scaffold for: an OLED/display driver, BLE peripheral role or
any "send data back to an app" functionality, or anything related to
next-chord prediction / ML. These may come later but aren't part of the
current work.

## Hardware

- MCU: ESP32-S3
- Piano: Roland FP-30X — has built-in Bluetooth MIDI, enabled by default (no
  external adapter needed)
- Output for now: USB serial back to this computer only

## Build / flash

The repo root is the ESP-IDF project (`project(espiano32)`). The IDF install
used is ESP-IDF v6.0.1 at `C:\esp\v6.0.1\esp-idf` (see
`.vscode/settings.json`).

```
idf.py set-target esp32s3      # once; sdkconfig.defaults does not pin the target
idf.py build
idf.py -p <PORT> flash monitor
idf.py menuconfig              # "Example Configuration" menu, see below
```

The VS Code ESP-IDF extension buttons are equivalent. The first build
downloads the managed components into `managed_components/`
— read those sources for `ble_conn_mgr` / `ble_midi` API details. There are
no tests or linters; verification is flashing and watching the serial log.

## Structure

- `main/app_main.c` — system init (NVS, default event loop) and the MIDI
  message consumer passed to `bluetooth_mgr_start()`. Keep it thin.
- `main/bluetooth_mgr.c/.h` — all BLE work, derived from the
  `ble_midi_central` example: scan/connect/pair/subscribe, the
  connection-manager event handler, reconnect on disconnect. Its only public
  API is `bluetooth_mgr_start(on_midi)`.
- New logic (note tracking, chord detection later) should go into its own
  `.c`/`.h` pair under `main/`, added to the `SRCS` list in
  `main/CMakeLists.txt`.
- `main/idf_component.yml` — declares the `ble_conn_mgr` / `ble_midi` /
  `ble_services` managed component dependencies from the example generator;
  don't hand-edit unless adding a genuinely new dependency.
- `main/Kconfig.projbuild` — `CONFIG_EXAMPLE_LOCAL_NAME` (our GAP name) and
  `CONFIG_EXAMPLE_PEER_NAME` (optional advertised name to match).
- `sdkconfig.defaults` — enables NimBLE, central role
  (`CONFIG_BLE_CONN_MGR_ROLE_CENTRAL`) and `CONFIG_BLE_MIDI_PROFILE`.

## MIDI data flow (bluetooth_mgr.c)

1. Scan callback (`scan_cb`) connects to the first advertiser
   whose adv data contains the BLE-MIDI 128-bit service UUID, **or** whose
   name equals `CONFIG_EXAMPLE_PEER_NAME` (empty = UUID-only). If the FP-30X
   isn't picked up, check whether it advertises the UUID and set the peer
   name via menuconfig.
2. `ESP_BLE_CONN_EVENT_CONNECTED` → MTU request (185);
   `ESP_BLE_CONN_EVENT_DISC_COMPLETE` → start Just Works pairing;
   `ESP_BLE_CONN_EVENT_ENC_CHANGE` → subscribe to MIDI I/O notifications.
   The FP-30X appears to refuse the subscribe on an unencrypted link.
   `ble_conn_mgr` never releases its wait when a GATT write fails, so a
   rejected write shows up as `ESP_ERR_TIMEOUT` after ~31 s.
3. `ESP_BLE_CONN_EVENT_DATA_RECEIVE` on the MIDI characteristic →
   `esp_ble_midi_on_bep_received()`, which parses the BLE-MIDI event packet
   and synchronously calls `midi_rx_cb` (raw packet, logged) and
   `midi_evt_cb` (one parsed MIDI message), which forwards to the app's
   `on_midi` callback.
4. `ESP_BLE_CONN_EVENT_DISCONNECTED` resets state and restarts scanning.
   There's no disconnect callback to the app yet. Add one when held-note
   state needs clearing.

## Conventions

- ESP-IDF only — no Arduino core, no Arduino libraries.
- Target chip: `esp32s3`.
- MIDI callbacks run synchronously inside the connection-manager event
  handler (the `esp_event` default loop task), i.e. on the BLE receive path —
  keep them short.
- For `DATA_RECEIVE`, `event_data` is an `esp_event`-owned copy: never free
  `conn_data->data`, and copy the bytes if another task needs them later.
- Note On with velocity 0 is a Note Off per the MIDI spec; handle both forms.
