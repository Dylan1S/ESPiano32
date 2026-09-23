
#pragma once


#include <stdint.h>
#include <stdbool.h>



typedef struct {uint32_t w[4]; } note_set_t;

bool note_tracker_handle(const uint8_t* msg, uint16_t len);

void note_tracker_get(note_set_t* out);

void note_tracker_reset(void);
