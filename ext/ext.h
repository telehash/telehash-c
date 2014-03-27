#ifndef ext_h
#define ext_h

#include <stdio.h>
#include "xht.h"
#include "chan.h"

void ext_link(chan_t c);

void ext_connect(chan_t c);

void ext_seek(chan_t c);

void ext_path(chan_t c);

typedef struct thtp_struct *thtp_t;
void ext_thtp(thtp_t t, chan_t c);
thtp_t thtp_new();
thtp_t thtp_free(thtp_t t);
thtp_t thtp_xht(thtp_t t, xht_t index);

#endif