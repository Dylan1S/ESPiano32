idf_component_register(SRCS "app_main.c" "bluetooth_mgr.c" "note_tracker.c" "note_set.c"
                       PRIV_REQUIRES nvs_flash bt esp_event)
