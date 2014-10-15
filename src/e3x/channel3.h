#ifndef channel3_h
#define channel3_h
#include <stdint.h>
#include "e3x.h"

typedef struct channel3_struct *channel3_t; // standalone channel packet management, buffering and ordering

// caller must manage lists of channels per exchange3 based on cid
channel3_t channel3_new(lob_t open); // open must be channel3_receive or channel3_send next yet
void channel3_free(channel3_t c);
void channel3_ev(channel3_t c, event3_t ev); // timers only work with this set

// incoming packets
uint8_t channel3_receive(channel3_t c, lob_t inner); // usually sets/updates event timer, ret if accepted/valid into receiving queue
void channel3_sync(channel3_t c, uint8_t sync); // false to force start timers (any new handshake), true to cancel and resend last packet (after any exchange3_sync)
lob_t channel3_receiving(channel3_t c); // get next avail packet in order, null if nothing

// outgoing packets
lob_t channel3_packet(channel3_t c);  // creates a packet w/ necessary json, just a convenience
uint8_t channel3_send(channel3_t c, lob_t inner); // adds to sending queue, adds json if needed
lob_t channel3_sending(channel3_t c); // must be called after every send or receive, pass pkt to exchange3_encrypt before sending

// convenience functions
uint32_t channel3_id(channel3_t c); // numeric of the open->cid
lob_t channel3_open(channel3_t c); // returns the open packet (always cached)
uint8_t channel3_state(channel3_t c); // E3CHAN_OPENING, E3CHAN_OPEN, or E3CHAN_ENDED
uint32_t channel3_size(channel3_t c); // size (in bytes) of buffered data in or out


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