#ifndef e3x_h
#define e3x_h

#include "lob.h" // json+binary container

// everything is prefixed with e3* to minimize symbol/naming conflicts

typedef struct e3self_struct *e3self_t;
typedef struct e3x_struct *e3x_t;
typedef struct e3ev_struct *e3ev_t;
typedef struct e3chan_struct *e3chan_t;

//################################
// local endpoint state management

// load keys to create a new local endpoint
e3self_t e3self_new(lob_t secrets);
void e3self_free(e3self_t e); // any e3x's must have been free'd first

// try to decrypt any message sent to us, returns the inner
// must also be passed to verify to validate sender
lob_t e3self_decrypt(e3self_t e, lob_t message);

// generate new local secrets
lob_t e3self_generate(void);

// these require a self (local) and an exchange (remote) but are exchange independent
lob_t e3self_x_encrypt(e3self_t e, e3x_t x, lob_t inner, uint32_t seq); // will safely set/increment seq if 0
uint8_t e3self_x_verify(e3self_t e, e3x_t x, lob_t message); // any handshake verify fail (lower seq), always resend handshake


//##################
// a single exchange (a session w/ local endpoint and remote endpoint)

// make a new exchange
// packet must contain the keys or a handshake to exchange with
e3x_t e3x_new(e3self_t e, lob_t with, uint32_t seq); // seq must be higher than any previous x used with them
void e3x_free(e3x_t x);

// simple accessor utilities
uint8_t *e3x_token(e3x_t x); // 16 bytes, unique to this exchange for matching/routing
uint32_t e3x_seq(e3x_t x); // last used seq value

// returns the seq value for a handshake reply if needed
// sets secrets/seq/cids to the given handshake if it's newer
// always call chan_sync(c,true) after this on all open channels to signal them the exchange is active
uint32_t e3x_sync(e3x_t x, lob_t handshake);

// just a convenience, seq=0 means force new handshake (and call chan_sync(false)), or seq = e3x_seq() or e3x_sync()
lob_t e3x_handshake(e3x_t x, lob_t inner, uint32_t seq);

// simple encrypt/decrypt conversion of any packet for channels
lob_t e3x_decrypt(e3x_t x, lob_t packet); // goes to channel, validates cid
lob_t e3x_encrypt(e3x_t x, lob_t inner); // comes from channel 

// get next avail outgoing channel id
uint32_t e3x_cid(e3x_t x);


//##################
// these are separate and entirely standalone as utilities that can be used optionally

// simple timer eventing (for channels)
e3ev_t e3ev_new();
uint32_t e3ev_at(e3ev_t ev); // when to call e3ev_get()
lob_t e3ev_get(e3ev_t ev); // has token and cid, look up and pass in to chan_receive
void e3ev_set(e3ev_t ev, lob_t event, uint32_t at); // 0 is delete, event is unique per ->id

// caller must manage lists of channels per e3x based on cid
e3chan_t e3chan_new(lob_t open); // open must be e3chan_receive or e3chan_send next yet
void e3chan_free(e3chan_t c);
void e3chan_ev(e3chan_t c, e3ev_t ev); // timers only work with this set

// incoming packets
uint8_t e3chan_receive(e3chan_t c, lob_t inner); // usually sets/updates event timer, ret if accepted/valid into receiving queue
void e3chan_sync(e3chan_t c, uint8_t sync); // false to force start timers (any new handshake), true to cancel and resend last packet (after any e3x_sync)
lob_t e3chan_receiving(e3chan_t c); // get next avail packet in order, null if nothing

// outgoing packets
lob_t e3chan_packet(e3chan_t c);  // creates a packet w/ necessary json, just a convenience
uint8_t e3chan_send(e3chan_t c, lob_t inner); // adds to sending queue, adds json if needed
lob_t e3chan_sending(e3chan_t c); // must be called after every send or receive, pass pkt to e3x_encrypt before sending

// convenience functions
uint32_t e3chan_id(e3chan_t c); // numeric of the open->cid
lob_t e3chan_open(e3chan_t c); // returns the open packet (always cached)
uint8_t e3chan_state(e3chan_t c); // E3CHAN_OPENING, E3CHAN_OPEN, or E3CHAN_ENDED
uint32_t e3chan_size(e3chan_t c); // size (in bytes) of buffered data in or out

//##############
/* binding notes

* app must have an index of the hashnames-to-exchange and tokens-to-exchange
* for each exchange, keep a list of active network transport sessions and potential transport paths
* also keep a list of active channels per exchange to track state
* one transport session may be delivering to multiple exchanges (not a 1:1 mapping)
* unreliable transport sessions must trigger a new handshake for any exchange when they need to be re-validated
* all transport sessions must signal when they are closed, which generates a new handshake to any other sessions
* always use the most recently validated-active transport session to deliver to for sending

*/

#endif