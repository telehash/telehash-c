#ifndef event3_h
#define event3_h

#include "../lib/lob.h"

// simple timer eventing (for channels) that can be replaced by different backends
// an event is just a lob packet and ordering value

typedef struct event3_struct *event3_t;

// create a list/group of events, give prime number to create a hashtable of that size for faster id lookups
event3_t event3_new(uint32_t prime);
void event3_free(event3_t ev);

// the next lowest at value, or 0 if none
uint32_t event3_at(event3_t ev);

// remove and return the lowest lob packet below the given at
lob_t event3_get(event3_t ev, uint32_t at);

// 0 is delete, event is unique per id
// id must be stored in the event somewhere, and this uses the event->quota, next, and chain values only
void event3_set(event3_t ev, lob_t event, char *id, uint32_t at);


#endif