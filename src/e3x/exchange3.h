#ifndef exchange3_h
#define exchange3_h

#include <stdint.h>
#include "cipher3.h"
#include "self3.h"

typedef struct exchange3_struct
{
  cipher3_t cs;
  self3_t self;
  uint8_t csid, order;
  char hex[3];
  remote_t remote;
  uint8_t token[16];
  uint32_t at, cid;
} *exchange3_t;

// make a new exchange
// packet must contain the raw key in the body
exchange3_t exchange3_new(self3_t self, uint8_t csid, lob_t key, uint32_t at);
void exchange3_free(exchange3_t x);

// these require a self (local) and an exchange (remote) but are exchange independent
lob_t exchange3_message(exchange3_t x, lob_t inner);
uint8_t exchange3_verify(exchange3_t x, lob_t outer);

// returns the seq value for a handshake reply if needed
// sets secrets/seq/cids to the given handshake if it's newer
// always call chan_sync(c,true) after this on all open channels to signal them the exchange is active
uint32_t exchange3_sync(exchange3_t x, lob_t handshake);

// just a convenience, seq=0 means force new handshake (and call chan_sync(false)), or seq = exchange3_seq() or exchange3_sync()
lob_t exchange3_handshake(exchange3_t x, lob_t inner, uint32_t at);

// simple encrypt/decrypt conversion of any packet for channels
lob_t exchange3_receive(exchange3_t x, lob_t outer); // goes to channel, validates cid
lob_t exchange3_send(exchange3_t x, lob_t inner); // comes from channel 

// get next avail outgoing channel id
uint32_t exchange3_cid(exchange3_t x);


#endif