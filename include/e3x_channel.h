#ifndef e3x_channel_h
#define e3x_channel_h
#include <stdint.h>
#include "e3x.h"

typedef struct e3x_channel_struct *e3x_channel_t; // standalone channel packet management, buffering and ordering

// caller must manage lists of channels per e3x_exchange based on cid
e3x_channel_t e3x_channel_new(lob_t open); // open must be e3x_channel_receive or e3x_channel_send next yet
void e3x_channel_free(e3x_channel_t c);

// sets new timeout, or returns current time left if 0
uint32_t e3x_channel_timeout(e3x_channel_t c, e3x_event_t ev, uint32_t timeout);

// sets the max size (in bytes) of all buffered data in or out, returns current usage, can pass 0 just to check
uint32_t e3x_channel_size(e3x_channel_t c, uint32_t max); // will actively signal incoming window size depending on capacity left

// incoming packets
uint8_t e3x_channel_receive(e3x_channel_t c, lob_t inner); // usually sets/updates event timer, ret if accepted/valid into receiving queue
void e3x_channel_sync(e3x_channel_t c, uint8_t sync); // false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_exchange_sync)
lob_t e3x_channel_receiving(e3x_channel_t c); // get next avail packet in order, null if nothing

// outgoing packets
lob_t e3x_channel_oob(e3x_channel_t c); // id/ack/miss only headers base packet
lob_t e3x_channel_packet(e3x_channel_t c);  // creates a sequenced packet w/ all necessary headers, just a convenience
uint8_t e3x_channel_send(e3x_channel_t c, lob_t inner); // adds to sending queue
lob_t e3x_channel_sending(e3x_channel_t c); // must be called after every send or receive, pass pkt to e3x_exchange_encrypt before sending

// convenience functions
char *e3x_channel_uid(e3x_channel_t c); // process-unique string id
uint32_t e3x_channel_id(e3x_channel_t c); // numeric of the open->c id
char *e3x_channel_c(e3x_channel_t c); // string of the c id
lob_t e3x_channel_open(e3x_channel_t c); // returns the open packet (always cached)

enum e3x_channel_states { ENDED, OPENING, OPEN };
enum e3x_channel_states e3x_channel_state(e3x_channel_t c);


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
*/


#endif
