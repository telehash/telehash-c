#ifndef ext_seek_h
#define ext_seek_h

#include "ext.h"

void seek_free(switch_t s);

// create a seek to this hn and initiate connect
void seek_connect(switch_t s, hn_t h);

// just call back note instead of auto-connect
void seek_note(switch_t s, hn_t h, packet_t note);

#endif
