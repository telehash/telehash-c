#ifndef e3x_h
#define e3x_h

// this includes generic buffer encoding/decoding to packets
// also defines packet_t that has a ->eid, ->cid, ->arg
#include "packet.h"

typedef struct e3x_struct
{
  uint8_t id[32];
  xht_t sids, eids;
} *e3x_t;

// pass in a prime to optimize the eid lookup table, 0 to use default
e3x_t e3x_new(uint32_t prime, packet_t keys);

// will immediately free everything
e3x_t e3x_free(e3x_t);

// parts must contain body of the key too
uint8_t *e3x_endpoint(packet_t parts);

// will error any active channels and then free any caches after
void e3x_drop(uint8_t *eid);

packet_t e3x_decrypt(uint8_t *eid, uint8_t *buffer, uint32_t len);
uint8_t *e3x_encrypt(uint8_t *eid, packet_t packet, uint8_t *len);

// processes data coming from any network interface
packet_t e3x_process(uint8_t *buffer, uint8_t len);

// only pass in packets returned from _process, if their ->eid is trusted
// returns 0 for dropped packets, 1 for new/valid ones
uint8_t e3x_receive(packet_t packet);

// use when the network transport needs to be re-validated for an eid
// generates a handshake if there's any channels, will re-send and start to timeout channels if not confirmed
uint8_t e3x_keepalive(uint8_t *eid, uint8_t *len);

// returns the next eid with any packets to send/receive
uint8_t *e3x_busy();

// get the next buffer to send to this eid (if any), caller free's
uint8_t *e3x_sending(uint8_t *eid, uint8_t *len);

// get the next packet received from this eid, will have ->cid for the channel
packet_t e3x_receiving(uint8_t *eid);

// create new reliable (1) or unreliable (2) channel id, timeout is for start/acks/ends
uint32_t e3x_channel(uint8_t *eid, uint8_t kind, uint32_t timeout);

// create a new packet to send to this channel
// packets created must be given to send() in order
packet_t e3x_packet(uint8_t *eid, uint32_t cid);

// encrypts and shows up in sending(), queues and triggers handshake too
// 0 if not delivered, else is # of packets waiting to be sent and/or ack'd
// reliable packets are returned in receiving() once delivered (track w/ ->id)
uint32_t e3x_send(packet_t packet);


#endif