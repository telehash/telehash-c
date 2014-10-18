#ifndef link_h
#define link_h
#include <stdint.h>

typedef struct link_struct *link_t;

#include "mesh.h"

struct link_struct
{
  // public link data
  hashname_t id;
  exchange3_t x;
  mesh_t mesh;

  // these are for internal link management only
  struct seen_struct *pipes;
};

link_t link_new(mesh_t mesh, hashname_t id);
void link_free(link_t link);

/*
// default channel inactivity timeout in seconds
#define CHAN_TIMEOUT 10

typedef struct link_struct
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
  struct link_struct *next;
  packet_t in, inend, notes;
  void *arg; // used by extensions
  void *seq, *miss; // used by link_seq/link_miss
  // event callbacks for channel implementations
  void (*handler)(struct link_struct*); // handle incoming packets immediately/directly
  void (*tick)(struct link_struct*); // called approx every tick (1s)
} *link_t;

// kind of a macro, just make a reliable channel of this type to this hashname
link_t link_start(struct switch_struct *s, char *hn, char *type);

// new channel, pass id=0 to create an outgoing one, is auto freed after timeout and ->arg == NULL
link_t link_new(struct switch_struct *s, struct hn_struct *to, char *type, uint32_t id);

// configures channel as a reliable one before any packets are sent, is max # of packets to buffer before backpressure
link_t link_reliable(link_t c, int window);

// resets channel state for a hashname (new line)
void link_reset(struct switch_struct *s, struct hn_struct *to);

// set an in-activity timeout for the channel (no incoming), will generate incoming {"err":"timeout"} if not ended
void link_timeout(link_t c, uint32_t seconds);

// just a convenience to send an end:true packet (use this instead of link_send())
void link_end(link_t c, packet_t p);

// sends an error packet immediately
link_t link_fail(link_t c, char *err);

// gives all channels a chance to do background stuff
void link_tick(struct switch_struct *s, struct hn_struct *to);

// returns existing or creates new and adds to from
link_t link_in(struct switch_struct *s, struct hn_struct *from, packet_t p);

// create a packet ready to be sent for this channel, returns NULL for backpressure
packet_t link_packet(link_t c);

// pop a packet from this channel to be processed, caller must free
packet_t link_pop(link_t c);

// get the next incoming note waiting to be handled
packet_t link_notes(link_t c);

// stamp or create (if NULL) a note as from this channel
packet_t link_note(link_t c, packet_t note);

// send the note back to the creating channel, frees note
int link_reply(link_t c, packet_t note);

// internal, receives/processes incoming packet
void link_receive(link_t c, packet_t p);

// smartly send based on what type of channel we are
void link_send(link_t c, packet_t p);

// optionally sends reliable channel ack-only if needed
void link_ack(link_t c);

// add/remove from switch processing queue
void link_queue(link_t c);
void link_dequeue(link_t c);

// just add ack/miss
packet_t link_seq_ack(link_t c, packet_t p);

// new sequenced packet, NULL for backpressure
packet_t link_seq_packet(link_t c);

// buffers packets until they're in order, 1 if some are ready to pop
int link_seq_receive(link_t c, packet_t p);

// returns ordered packets for this channel, updates ack
packet_t link_seq_pop(link_t c);

void link_seq_init(link_t c);
void link_seq_free(link_t c);

// tracks packet for outgoing, eventually free's it, 0 ok or 1 for full/backpressure
int link_miss_track(link_t c, uint32_t seq, packet_t p);

// resends all waiting packets
void link_miss_resend(link_t c);

// looks at incoming miss/ack and resends or frees
void link_miss_check(link_t c, packet_t p);

void link_miss_init(link_t c);
void link_miss_free(link_t c);
*/

#endif