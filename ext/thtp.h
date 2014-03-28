#ifndef ext_thtp_h
#define ext_thtp_h

#include "xht.h"
#include "chan.h"

typedef struct thtp_struct *thtp_t;

void ext_thtp(thtp_t t, chan_t c);
thtp_t thtp_new();
thtp_t thtp_free(thtp_t t);
void thtp_glob(thtp_t t, char *glob, packet_t note);
void thtp_path(thtp_t t, char *path, packet_t note);

#endif