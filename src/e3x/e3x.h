#ifndef e3x_h
#define e3x_h

// defines packet_t
// this includes generic buffer encoding/decoding to packets and json/body access
#include "packet.h"

//################################
// local endpoint state management

typedef struct e3x_self_struct *e3x_self_t;

// load keys to create a new local endpoint
e3x_self_t e3x_self_new(packet_t secrets);
void e3x_self_free(e3x_self_t self);

// processes data coming from any network interface
// packet has either a token or contains a handshake
packet_t e3x_self_receive(e3x_self_t self, packet_t packet);

// generate new local secrets
packet_t e3x_self_generate(void);


//##################
// a single exchange

typedef struct e3x_struct *e3x_t;

// make a new exchange
// packet must contain the keys or a handshake to exchange with
e3x_t e3x_new(e3x_self_t self, packet_t with);

// will error any active channels and then free any caches after
void e3x_free(e3x_t e);

// get the 16 byte unique id for this exchange
uint8_t *e3x_token(e3x_t e);

// simple encrypt/decrypt conversion of any packet for async messaging patterns
packet_t e3x_decrypt(e3x_t e, packet_t packet);
packet_t e3x_encrypt(e3x_t e, packet_t packet);

// pass in packets returned from e3x_self_receive, only receive handshakes that are trusted
// returns:
//  0 - dropped/invalid
//  >0 - processed, a positive signal about the transport validity
//  1 - call e3x_sending, packets waiting to be sent
//  2 - call e3x_receiving, packets waiting to be received
//  3 - call both
uint8_t e3x_receive(e3x_t e, packet_t packet);

// use when the network transport needs to be re-validated for an exchange
// generates a handshake if there's any channels, will re-send and start to timeout channels if not confirmed
packet_t e3x_keepalive(e3x_t e);

// get the next packet to deliver for this exchange (if any), when it returns null use e3x_sending_next
packet_t e3x_sending(e3x_t e);

// returns the number of milliseconds until e3x_sending needs to be called again, or 0 if not
uint32_t e3x_sending_next(e3x_t e);

// get the next packet received in this exchange, will always have a channel id included
// when it returns null use e3x_receiving_next
packet_t e3x_receiving(e3x_t e);

// always returns the number of milliseconds until e3x_receiving needs to be called again, or 0 if not
uint32_t e3x_receiving_next(e3x_t e);

// create new reliable (1) or unreliable (2) channel, timeout is for start/acks/ends
// returns the id unique to this exchange only
uint32_t e3x_channel(e3x_t e, uint8_t kind, uint32_t timeout);

// create a new packet to send to this channel
packet_t e3x_packet(e3x_t e, uint32_t channel);

// encrypts and shows up in sending(), queues and triggers handshake too, always call e3x_sending after
// 0 if not delivered, else is # of packets waiting to be sent and/or ack'd in the queue
// reliable packets are returned in receiving() once delivered (track w/ "seq" id)
uint32_t e3x_send(packet_t packet);


//##############
/* binding notes

* app must have an index of the hashnames-to-exchange and tokens-to-exchange
* uses e3x_self_receive first for all incoming to get the key to either index and find an exchange
* uses a scheduler to know when to check any exchange
* for each exchange, keep a list of active network transport sessions and potential transport paths
* also keep a list of active channels per exchange to track state
* one transport session may be delivering to multiple exchanges (not a 1:1 mapping)
* unreliable transport sessions must signal to any exchange when they need a keepalive to be validated
* sessions must signal when they are closed, which generates a keepalive to any other sessions
* always use the most recently active session to deliver to for sending

*/

#endif