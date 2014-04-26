#ifndef chan_h
#define chan_h
#include <stdint.h>
#include "packet.h"

#define CHAN_STARTING 1
#define CHAN_OPEN 2
#define CHAN_ENDING 3
#define CHAN_ENDED 0

typedef struct chan_struct
{
  uint32_t id;
  unsigned char hexid[9], uid[9];
  struct switch_struct *s;
  struct hn_struct *to;
  char *type;
  int reliable;
  uint8_t state;
  struct path_struct *last;
  struct chan_struct *next;
  packet_t in, inend, notes;
  void *arg; // used by app
  void *seq, *miss; // used by chan_seq/chan_miss
  void (*handler)(struct chan_struct*); // auto-fire callback
} *chan_t;

// kind of a macro, just make a reliable channel of this type to this hashname
chan_t chan_start(struct switch_struct *s, char *hn, char *type);

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

// pop a packet from this channel to be processed, caller must free
packet_t chan_pop(chan_t c);

// flags channel as gracefully ended, optionally adds end to packet
chan_t chan_end(chan_t c, packet_t p);

// immediately fails/removes channel, if err tries to send message
chan_t chan_fail(chan_t c, char *err);

// get the next incoming note waiting to be handled
packet_t chan_notes(chan_t c);

// stamp or create (if NULL) a note as from this channel
packet_t chan_note(chan_t c, packet_t note);

// send the note back to the creating channel, frees note
int chan_reply(chan_t c, packet_t note);

// internal, receives/processes incoming packet
void chan_receive(chan_t c, packet_t p);

// smartly send based on what type of channel we are
void chan_send(chan_t c, packet_t p);

// optionally sends reliable channel ack-only if needed
void chan_ack(chan_t c);

// add/remove from switch processing queue
void chan_queue(chan_t c);
void chan_dequeue(chan_t c);

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
int chan_miss_track(chan_t c, uint32_t seq, packet_t p);

// buffers packets to be able to re-send
void chan_miss_send(chan_t c, packet_t p);

// looks at incoming miss/ack and resends or frees
void chan_miss_check(chan_t c, packet_t p);

void chan_miss_init(chan_t c);
void chan_miss_free(chan_t c);

#endif