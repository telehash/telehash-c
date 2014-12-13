#ifndef e3x_event_h
#define e3x_event_h

#include "lob.h"

// simple timer eventing (for channels) that can be replaced by different backends
// an event is just a lob packet and ordering value

typedef struct e3x_event_struct *e3x_event_t;

// create a list/group of events, give prime number to create a hashtable of that size for faster id lookups
e3x_event_t e3x_event_new(uint32_t prime, uint64_t now);
void e3x_event_free(e3x_event_t ev);

// the number of ms in the future from now for the next event (for scheduling), 0 if events waiting, max-uint32 if none
uint32_t e3x_event_at(e3x_event_t ev, uint64_t now);

// remove and return the oldest event, after an event_at returned 0
lob_t e3x_event_get(e3x_event_t ev);

// 0 is delete, event is unique per id
// id must be stored in the event somewhere, and this uses the event->id, next, and chaining values only
// wait is ms in the future from now
void e3x_event_set(e3x_event_t ev, lob_t event, char *id, uint32_t wait);


#endif
