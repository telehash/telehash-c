#ifndef chan_h
#define chan_h
#include <stdint.h>
#include "packet.h"

typedef struct chan_struct
{
  uint32_t id;
  unsigned char hexid[9];
  struct switch_struct *s;
  struct hn_struct *to;
  char *type;
  int reliable;
  enum {STARTING, OPEN, ENDED} state;
  struct path_struct *last;
  struct chan_struct *next;
  packet_t in, inend;
  void *seq, *miss; // used by chan_seq/chan_miss
} *chan_t;

chan_t chan_new(struct switch_struct *s, struct hn_struct *to, char *type, int reliable);
void chan_free(chan_t c);

// resets channel state for a hashname
void chan_reset(struct switch_struct *s, struct hn_struct *to);

// returns existing or creates new and adds to from
chan_t chan_in(struct switch_struct *s, struct hn_struct *from, packet_t p);

// create a packet ready to be sent for this channel, returns NULL for backpressure
packet_t chan_packet(chan_t c);

// send a packet out on this channel
void chan_send(chan_t c, packet_t p);

// pop a packet from this channel to be processed, caller must free
packet_t chan_pop(chan_t c);

// internal, receives/processes incoming packet
void chan_receive(chan_t c, packet_t p);

// adds seq, ack, miss
void chan_seq_add(chan_t c, packet_t p);

// buffers packets until they're in order
void chan_seq_receive(chan_t c, packet_t p);

// returns ordered packets for this channel, updates ack
packet_t chan_seq_pop(chan_t c);

void chan_seq_init(chan_t c);
void chan_seq_free(chan_t c);

// buffers packets to be able to re-send
void chan_miss_send(chan_t c, packet_t p);

// looks at incoming miss/ack and resends or frees
void chan_miss_check(chan_t c, packet_t p);

void chan_miss_init(chan_t c);
void chan_miss_free(chan_t c);

#endif