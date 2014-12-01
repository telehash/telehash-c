#ifndef ext_seek_h
#define ext_seek_h

#include "ext.h"

// this will set up the switch to auto-connect hashnames on demand
void seek_auto(switch_t s);

void seek_free(switch_t s);

// just call back note instead of auto-connect
void seek_note(switch_t s, hashname_t h, lob_t note);

#endif
