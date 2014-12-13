#ifndef event3_h
#define event3_h

#include "lob.h"

// simple timer eventing (for channels) that can be replaced by different backends
// an event is just a lob packet and ordering value

typedef struct event3_struct *event3_t;

// create a list/group of events, give prime number to create a hashtable of that size for faster id lookups
event3_t event3_new(uint32_t prime, uint64_t now);
void event3_free(event3_t ev);

// the number of ms in the future from now for the next event (for scheduling), 0 if events waiting, max-uint32 if none
uint32_t event3_at(event3_t ev, uint64_t now);

// remove and return the oldest event, after an event_at returned 0
lob_t event3_get(event3_t ev);

// 0 is delete, event is unique per id
// id must be stored in the event somewhere, and this uses the event->id, next, and chaining values only
// wait is ms in the future from now
void event3_set(event3_t ev, lob_t event, char *id, uint32_t wait);


#endif