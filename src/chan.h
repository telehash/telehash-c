#ifndef chan_h
#define chan_h

#include "switch.h"

typedef struct chan_struct
{
  unsigned char id[16];
  char hexid[33];
  struct switch_struct *s;
  struct hn_struct *to;
  char *type;
  int reliable;
  enum {STARTING, OPEN, ENDED} state;
  path_t last;
  struct chan_struct *next;
} *chan_t;

chan_t chan_new(struct switch_struct *s, struct hn_struct *to, char *type, int reliable);
void chan_free(chan_t c);

// create a packet ready to be sent for this channel
struct packet_struct *chan_packet(chan_t c);

// internal, receives/processes incoming packet
void chan_receive(chan_t c, struct packet_struct *p);

#endif