#ifndef chan_h
#define chan_h
#include <stdint.h>
#include "packet.h"

// default channel inactivity timeout in seconds
#define CHAN_TIMEOUT 10

typedef struct chan_struct
{
  uint32_t id; // wire id (not unique)
  uint32_t tsent, trecv; // tick value of last send, recv
  uint32_t timeout; // seconds since last trecv to auto-err
  unsigned char hexid[9], uid[9]; // uid is switch-wide unique
  struct switch_struct *s;
  struct hn_struct *to;
  char *type;
  int reliable;
  uint8_t opened, ended;
  uint8_t tfree, tresend; // tick counts for thresholds
  struct chan_struct *next;
  packet_t in, inend, notes;
  void *arg; // used by extensions
  void *seq, *miss; // used by chan_seq/chan_miss
  // event callbacks for channel implementations
  void (*handler)(struct chan_struct*); // handle incoming packets immediately/directly
  void (*tick)(struct chan_struct*); // called approx every tick (1s)
} *chan_t;

// kind of a macro, just make a reliable channel of this type to this hashname
chan_t chan_start(struct switch_struct *s, char *hn, char *type);

// new channel, pass id=0 to create an outgoing one, is auto freed after timeout and ->arg == NULL
chan_t chan_new(struct switch_struct *s, struct hn_struct *to, char *type, uint32_t id);

// configures channel as a reliable one before any packets are sent, is max # of packets to buffer before backpressure
chan_t chan_reliable(chan_t c, int window);

// resets channel state for a hashname (new line)
void chan_reset(struct switch_struct *s, struct hn_struct *to);

// set an in-activity timeout for the channel (no incoming), will generate incoming {"err":"timeout"} if not ended
void chan_timeout(chan_t c, uint32_t seconds);

// just a convenience to send an end:true packet (use this instead of chan_send())
void chan_end(chan_t c, packet_t p);

// sends an error packet immediately
chan_t chan_fail(chan_t c, char *err);

// gives all channels a chance to do background stuff
void chan_tick(struct switch_struct *s, struct hn_struct *to);

// returns existing or creates new and adds to from
chan_t chan_in(struct switch_struct *s, struct hn_struct *from, packet_t p);

// create a packet ready to be sent for this channel, returns NULL for backpressure
packet_t chan_packet(chan_t c);

// pop a packet from this channel to be processed, caller must free
packet_t chan_pop(chan_t c);

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

// resends all waiting packets
void chan_miss_resend(chan_t c);

// looks at incoming miss/ack and resends or frees
void chan_miss_check(chan_t c, packet_t p);

void chan_miss_init(chan_t c);
void chan_miss_free(chan_t c);

#endif