#ifndef ext_thtp_h
#define ext_thtp_h

/*
#include "xht.h"
#include "chan.h"

void ext_thtp(chan_t c);

// optionally initialize thtp w/ an index, happens automatically too
void thtp_init(switch_t s, xht_t index);
void thtp_free(switch_t s);

// sends requests matching this glob ("/path" matches "/path/foo") to this note, most specific wins
void thtp_glob(switch_t s, char *glob, lob_t note);

// sends requests matching this exact path to this note
void thtp_path(switch_t s, char *path, lob_t note);

// generate an outgoing request, send the response attached to the note
chan_t thtp_req(switch_t s, lob_t note);
*/
#endif
