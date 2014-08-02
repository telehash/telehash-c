#ifndef e3x_h
#define e3x_h

// this includes generic buffer encoding/decoding to packets
// also defines packet_t that has a ->token, ->arg
#include "packet.h"

// hn_t
#include "hashname.h"


// my id and crypto keys
typedef struct e3x_self_struct *e3x_self_t;
e3x_self_t e3x_self_new(packet_t keys);
void e3x_self_free(e3x_self_t);

// a single exchange
typedef struct e3x_struct *e3x_t;

// to must contain keys or a handshake
// make a new exchange
e3x_t e3x_new(e3x_self_t self, packet_t to);

// will error any active channels and then free any caches after
void e3x_free(e3x_t e);

// get the 16 byte unique id
uint8_t *e3x_token(e3x_t e);

// simple encrypt/decrypt conversion of a packet
packet_t e3x_decrypt(e3x_t e, packet_t packet);
packet_t e3x_encrypt(e3x_t e, packet_t packet);

// processes data coming from any network interface
// packet has either ->token or contains a handshake
packet_t e3x_self_receive(e3x_self_t self, packet_t packet);

// only pass in packets returned from _receive, only handshakes that are trusted
// returns 0 for dropped packets, 1 for new/valid ones
uint8_t e3x_receive(e3x_t e, packet_t packet);

// use when the network transport needs to be re-validated for an exchange
// generates a handshake if there's any channels, will re-send and start to timeout channels if not confirmed
packet_t e3x_keepalive(e3x_t e);

// returns the next one with any packets to send/receive
uint32_t e3x_next(e3x_t e);

// get the next packet to send to them (if any), caller free's
packet_t e3x_sending(e3x_t e);

// get the next packet received from them, will have ->cid for the channel
packet_t e3x_receiving(e3x_t e);

// create new reliable (1) or unreliable (2) channel id, timeout is for start/acks/ends
uint32_t e3x_channel(e3x_t e, uint8_t kind, uint32_t timeout);

// create a new packet to send to this channel
// packets created must be given to send() in order
packet_t e3x_packet(e3x_t e, uint32_t cid);

// encrypts and shows up in sending(), queues and triggers handshake too
// 0 if not delivered, else is # of packets waiting to be sent and/or ack'd
// reliable packets are returned in receiving() once delivered (track w/ ->id)
uint32_t e3x_send(packet_t packet);


#endif