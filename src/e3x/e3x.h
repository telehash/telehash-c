#ifndef e3x_h
#define e3x_h

#include "lob.h" // json+binary container

// ##### e3x - end-to-end encrypted exchange #####
//
// * intended to be wrapped/embedded in other codebases
// * includes some minimal crypto, but is primarily a wrapper around other crypto libraries
// * tries to minimize the integration points to send and receive encrypted packets over any transport
//

// everything contains a '3' to minimize any naming conflicts when used with other codebases
typedef struct self3_struct *self3_t; // this endpoint
typedef struct exchange3_struct *exchange3_t; // an exchange with another endpoint
typedef struct channel3_struct *channel3_t; // standalone channel packet management, buffering and ordering
typedef struct event3_struct *event3_t; // standalone event timer utility


// ###########################
// top-level library functions

// process-wide boot one-time initialization, !0 is error and lob_get(options,"err");
uint8_t e3x_init(lob_t options);

// return last known error string, if any
uint8_t *e3x_err(void);

// generate a new local identity, secrets returned in the lob json and keys in the linked lob json
lob_t e3x_generate(void);

// random bytes, from a supported cipher set
uint8_t *e3x_rand(uint8_t *bytes, uint32_t len);

// sha256 hashing, from one of the cipher sets
uint8_t *e3x_hash(uint8_t *in, uint32_t len, uint8_t *out32);


//################################
// local endpoint state management

// load secrets/keys to create a new local endpoint
self3_t self3_new(lob_t secrets);
void self3_free(self3_t e); // any exchanges must have been free'd first

// try to decrypt any message sent to us, returns the inner
lob_t self3_decrypt(self3_t e, lob_t message);


//##################
// a single exchange (a session w/ local endpoint and remote endpoint)

// make a new exchange
// packet must contain the keys or a handshake to exchange with
exchange3_t exchange3_new(self3_t e, lob_t with, uint32_t seq); // seq must be higher than any previous x used with them
void exchange3_free(exchange3_t x);

// simple accessor utilities
uint8_t *exchange3_token(exchange3_t x); // 16 bytes, unique to this exchange for matching/routing
uint32_t exchange3_seq(exchange3_t x); // last used seq value

// these require a self (local) and an exchange (remote) but are exchange independent
lob_t exchange3_message(exchange3_t x, lob_t inner, uint32_t seq); // will safely set/increment seq if 0
uint8_t exchange3_verify(exchange3_t x, lob_t message); // any handshake verify fail (lower seq), always resend handshake

// returns the seq value for a handshake reply if needed
// sets secrets/seq/cids to the given handshake if it's newer
// always call chan_sync(c,true) after this on all open channels to signal them the exchange is active
uint32_t exchange3_sync(exchange3_t x, lob_t handshake);

// just a convenience, seq=0 means force new handshake (and call chan_sync(false)), or seq = exchange3_seq() or exchange3_sync()
lob_t exchange3_handshake(exchange3_t x, lob_t inner, uint32_t seq);

// simple encrypt/decrypt conversion of any packet for channels
lob_t exchange3_receive(exchange3_t x, lob_t packet); // goes to channel, validates cid
lob_t exchange3_send(exchange3_t x, lob_t inner); // comes from channel 

// get next avail outgoing channel id
uint32_t exchange3_cid(exchange3_t x);


//##################################################
// standalone channel packet buffer/ordering utility

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


//############################################
// standalone timer event utility for channels

// simple timer eventing (for channels)
event3_t event3_new();
uint32_t event3_at(event3_t ev); // when to call event3_get()
lob_t event3_get(event3_t ev); // has token and cid, look up and pass in to chan_receive
void event3_set(event3_t ev, lob_t event, uint32_t at); // 0 is delete, event is unique per ->id


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