#pragma once

#include <stdbool.h>
#include <stdint.h>



typedef struct {uint32_t notes[4]; } note_set_t;


enum notes
{
    NOTE_C, 
    NOTE_CS, 
    NOTE_D, 
    NOTE_DS, 
    NOTE_E, 
    NOTE_ES, 
    NOTE_F, 
    NOTE_FS, 
    NOTE_G, 
    NOTE_GS, 
    NOTE_A, 
    NOTE_AS, 
    NOTE_B, 
    NOTE_BS
};

static inline bool note_set_add(note_set_t* s, uint8_t note) 
{
    if(s == NULL) return false; 
    s->notes[note / 32] |= (1u << (note % 32));
    return true;
}
static inline bool note_set_remove(note_set_t* s, uint8_t note) 
{
    if(s == NULL) return false; 
    s->notes[note / 32] &= ~(1u << (note % 32));
    return true;
}

static inline bool note_set_has(const note_set_t* s, uint8_t note) 
{ 
    return s->notes[note / 32] & (1u << (note % 32));
}

/**
 * @return the number of bytes it wrote
 */
size_t note_set_format(const note_set_t* s, char *dest, size_t len);

const char *note_pitch_name(uint8_t note);



