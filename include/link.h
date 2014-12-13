#ifndef link_h
#define link_h
#include <stdint.h>

typedef struct link_struct *link_t;

#include "mesh.h"

struct link_struct
{
  // public link data
  hashname_t id;
  e3x_exchange_t x;
  mesh_t mesh;
  lob_t key;
  uint8_t csid;
  xht_t index, channels;
  char token[33];
  
  // these are for internal link management only
  struct seen_struct *pipes;
};

// these all create or return existing one from the mesh
link_t link_get(mesh_t mesh, char *hashname);
link_t link_keys(mesh_t mesh, lob_t keys); // adds in the right key
link_t link_key(mesh_t mesh, lob_t key); // adds in from the body

// removes from mesh
void link_free(link_t link);

// load in the key to existing link
link_t link_load(link_t link, uint8_t csid, lob_t key);

// try to turn a path into a pipe and add it to the link
pipe_t link_path(link_t link, lob_t path);

// just add a pipe directly
link_t link_pipe(link_t link, pipe_t pipe);

// process an incoming handshake
link_t link_handshake(link_t link, lob_t inner, lob_t outer, pipe_t pipe);

// process a decrypted channel packet
link_t link_receive(link_t link, lob_t inner, pipe_t pipe);

// try to deliver this packet to the best pipe
link_t link_send(link_t link, lob_t inner);

// make sure current handshake is sent to all pipes
link_t link_sync(link_t link);

// trigger a new sync
link_t link_resync(link_t link);

// can channel data be sent/received
link_t link_ready(link_t link);

// create/track a new channel for this open
e3x_channel_t link_channel(link_t link, lob_t open);

// set up internal handler for all incoming packets on this channel
link_t link_handle(link_t link, e3x_channel_t c3, void (*handle)(link_t link, e3x_channel_t c3, void *arg), void *arg);

// encrpt and send any outgoing packets for this channel, send the inner if given
link_t link_flush(link_t link, e3x_channel_t c3, lob_t inner);

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
