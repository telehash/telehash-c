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



#endif
