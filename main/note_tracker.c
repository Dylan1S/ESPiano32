
#include "note_tracker.h"
#include "esp_log.h"

/**
 * set containing all notes currently held down
 */
static note_set_t s_held;

const char* TAG = "NOTE_TRACKER";
// forward declarations of all functions


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

enum midi_message_status : uint8_t
{
    STATUS_NOTE_OFF     = 0x8,
    STATUS_NOTE_ON      = 0x9,
    STATUS_CONTROLLER   = 0xB,
    STATUS_POLYPHONIC_AFTERTOUCH = 0xA, //might not need this, unsure 
};


//Lookup table for all notes. Any note can be found with k_pitch_notes[note % 12]
static const char *const k_pitch_notes[12] = 
{
    "C", "C#", "D", "D#", "E", "E#", "F", "F#", "G", "G#" "A", "A#", "B", "B#"
};



static void set_note(note_set_t *s, uint8_t note);

static void clear_note(note_set_t *s, uint8_t note);



bool note_tracker_handle(const uint8_t* msg, uint16_t len)
{
    //extract the message header - top 4 bits of msg[0]
    uint8_t msg_status = msg[0] & 0xf0;
    switch (msg_status)
    {
        case STATUS_NOTE_ON:
        {
            //if velocity is greater than 0 then add note to held set
            //else if velocity is 0 then acts as a note off event
            if(msg[2] > 0)
            {
                set_note(&s_held, msg[1]);
            }
            else
            {
                clear_note(&s_held, msg[1]);
            }
            break;
        }
        case STATUS_NOTE_OFF:
        {
            //should just clear note - must find out whether velocity is relevant for this
            clear_note(&s_held, msg[1]);
            break;
        }
        case STATUS_CONTROLLER:
        {
            //TODO handle pedal events
            break;
        }
        default:
        {
            //Just log no handler and move on
            ESP_LOGI(TAG, "No handler supported for event %d", msg[0]);
            break;
        }
    }
}






