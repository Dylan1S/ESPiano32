
#include "note_tracker.h"
#include "note_set.h"
#include "esp_log.h"

/**
 * set containing all notes currently held down
 */
static note_set_t s_held = {0};

const char* TAG = "NOTE_TRACKER";
// forward declarations of all functions



enum midi_message_status : uint8_t
{
    STATUS_NOTE_OFF     = 0x80,
    STATUS_NOTE_ON      = 0x90,
    STATUS_CONTROLLER   = 0xB0,
    STATUS_POLYPHONIC_AFTERTOUCH = 0xA, //might not need this, unsure 
};


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
            bool success = false;
            if(msg[2] > 0)
            {
                success = note_set_add(&s_held, msg[1]);
                if(!success) return false;
            }
            else
            {
                success = note_set_remove(&s_held, msg[1]);
                if(!success) return false;
            }
            break;
        }
        case STATUS_NOTE_OFF:
        {
            bool success = false;
            //should just clear note - must find out whether velocity is relevant for this
            success = note_set_remove(&s_held, msg[1]);
            if(!success) return false;
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
            return false;
            break;
        }
    }
    return true;
}

note_set_t note_tracker_get(void)
{
    return s_held;
}





