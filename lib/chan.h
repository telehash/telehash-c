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

// new channel, pass id=0 to create an outgoing one
chan_t chan_new(struct switch_struct *s, struct hn_struct *to, char *type, uint32_t id);
void chan_free(chan_t c);

// configures channel as a reliable one, must be in STARTING state, is max # of packets to buffer before backpressure
chan_t chan_reliable(chan_t c, int window);

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

// just add ack/miss
packet_t chan_seq_ack(chan_t c, packet_t p);

// new sequenced packet, NULL for backpressure
packet_t chan_seq_packet(chan_t c);

// buffers packets until they're in order, 1 if some are ready to pop
int chan_seq_receive(chan_t c, packet_t p);

// returns ordered packets for this channel, updates ack
packet_t chan_seq_pop(chan_t c);

void chan_seq_init(chan_t c);
void chan_seq_free(chan_t c);

// tracks packet for outgoing, eventually free's it, 0 ok or 1 for full/backpressure
int chan_miss_track(chan_t c, int id, packet_t p);

// buffers packets to be able to re-send
void chan_miss_send(chan_t c, packet_t p);

// looks at incoming miss/ack and resends or frees
void chan_miss_check(chan_t c, packet_t p);

void chan_miss_init(chan_t c);
void chan_miss_free(chan_t c);

#endif