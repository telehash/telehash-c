#ifndef e3chan_h
#define e3chan_h
#include <stdint.h>
#include "e3x.h"

// default channel inactivity timeout in seconds
#define CHAN_TIMEOUT 10

struct e3chan_struct
{
  uint32_t id; // wire id (not unique)
  uint32_t tsent, trecv; // tick value of last send, recv
  uint32_t timeout; // seconds since last trecv to auto-err
  unsigned char hexid[9], uid[9]; // uid is switch-wide unique
  char *type;
  int reliable;
  uint8_t opened, ended;
  uint8_t tfree, tresend; // tick counts for thresholds
  struct e3chan_struct *next;
  lob_t in, inend, notes;
  void *arg; // used by extensions
  void *seq, *miss; // used by e3chan_seq/e3chan_miss
  // event callbacks for channel implementations
  void (*handler)(struct e3chan_struct*); // handle incoming packets immediately/directly
  void (*tick)(struct e3chan_struct*); // called approx every tick (1s)
};



/*

// kind of a macro, just make a reliable channel of this type to this hashname
e3chan_t e3chan_start(struct switch_struct *s, char *hn, char *type);

// new channel, pass id=0 to create an outgoing one, is auto freed after timeout and ->arg == NULL
e3chan_t e3chan_new(struct switch_struct *s, struct hashname_struct *to, char *type, uint32_t id);

// configures channel as a reliable one before any packets are sent, is max # of packets to buffer before backpressure
e3chan_t e3chan_reliable(e3chan_t c, int window);

// resets channel state for a hashname (new line)
void e3chan_reset(struct switch_struct *s, struct hashname_struct *to);

// set an in-activity timeout for the channel (no incoming), will generate incoming {"err":"timeout"} if not ended
void e3chan_timeout(e3chan_t c, uint32_t seconds);

// just a convenience to send an end:true packet (use this instead of e3chan_send())
void e3chan_end(e3chan_t c, lob_t p);

// sends an error packet immediately
e3chan_t e3chan_fail(e3chan_t c, char *err);

// gives all channels a chance to do background stuff
void e3chan_tick(struct switch_struct *s, struct hashname_struct *to);

// returns existing or creates new and adds to from
e3chan_t e3chan_in(struct switch_struct *s, struct hashname_struct *from, lob_t p);

// create a packet ready to be sent for this channel, returns NULL for backpressure
lob_t e3chan_packet(e3chan_t c);

// pop a packet from this channel to be processed, caller must free
lob_t e3chan_pop(e3chan_t c);

// get the next incoming note waiting to be handled
lob_t e3chan_notes(e3chan_t c);

// stamp or create (if NULL) a note as from this channel
lob_t e3chan_note(e3chan_t c, lob_t note);

// send the note back to the creating channel, frees note
int e3chan_reply(e3chan_t c, lob_t note);

// internal, receives/processes incoming packet
void e3chan_receive(e3chan_t c, lob_t p);

// smartly send based on what type of channel we are
void e3chan_send(e3chan_t c, lob_t p);

// optionally sends reliable channel ack-only if needed
void e3chan_ack(e3chan_t c);

// add/remove from switch processing queue
void e3chan_queue(e3chan_t c);
void e3chan_dequeue(e3chan_t c);
*/

// just add ack/miss
lob_t e3chan_seq_ack(e3chan_t c, lob_t p);

// new sequenced packet, NULL for backpressure
lob_t e3chan_seq_packet(e3chan_t c);

// buffers packets until they're in order, 1 if some are ready to pop
int e3chan_seq_receive(e3chan_t c, lob_t p);

// returns ordered packets for this channel, updates ack
lob_t e3chan_seq_pop(e3chan_t c);

void e3chan_seq_init(e3chan_t c);
void e3chan_seq_free(e3chan_t c);

// tracks packet for outgoing, eventually free's it, 0 ok or 1 for full/backpressure
int e3chan_miss_track(e3chan_t c, uint32_t seq, lob_t p);

// resends all waiting packets
void e3chan_miss_resend(e3chan_t c);

// looks at incoming miss/ack and resends or frees
void e3chan_miss_check(e3chan_t c, lob_t p);

void e3chan_miss_init(e3chan_t c);
void e3chan_miss_free(e3chan_t c);

#endif