
#pragma once


#include <stdint.h>
#include <stdbool.h>
#include "note_set.h"



bool note_tracker_handle(const uint8_t* msg, uint16_t len);

note_set_t note_tracker_get(void);

void note_tracker_reset(void);
