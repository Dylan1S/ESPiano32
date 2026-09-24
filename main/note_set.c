#include "note_set.h"
#include "stdio.h"


//Lookup table for all notes. Any note can be found with k_pitch_notes[note % 12]
static const char *const k_pitch_notes[12] = 
{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

const char *note_pitch_name(uint8_t note)
{
    return k_pitch_notes[note%12];
}



size_t note_set_format(const note_set_t* s, char* dest, size_t len)
{
    size_t pos = 0;
    dest[0] = '\0';
    for(int n = 0; n < 128; ++n)
    {
        if(!note_set_has(s, n)) continue;
        int written = snprintf(dest + pos, len - pos, "%s%s%d",
                            pos ? " ": "",
                            note_pitch_name(n % 12), n / 12 - 1);
        if (written < 0 || (size_t)written >= len - pos) break;
        pos += written;
    }
    if (pos == 0) snprintf(dest, len, "(none)");
    return pos;
}