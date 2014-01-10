#ifndef chan_h
#define chan_h

#include "switch.h"

typedef struct chan_struct
{
  struct switch_struct *s;
  hn_t h;
  char *type;
  int reliable;
  enum {STARTING, OPEN, ENDED} state;
  path_t last;
  struct chan_struct *next;
} *chan_t;

chan_t chan_new(struct switch_struct *s, hn_t h, char *type, int reliable);
void chan_free(chan_t c);

// create a packet ready to be sent for this channel
packet_t chan_packet(chan_t c);

// internal, receives/processes incoming packet
void chan_receive(chan_t c);

#endif