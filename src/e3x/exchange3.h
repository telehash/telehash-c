#ifndef exchange3_h
#define exchange3_h

#include <stdint.h>
#include "cipher3.h"
#include "self3.h"

typedef struct exchange3_struct
{
  cipher3_t cs;
  remote_t remote;
  uint8_t token[16];
  uint32_t at;
} *exchange3_t;

// make a new exchange
// packet must contain the keys or a handshake to exchange with
exchange3_t exchange3_new(self3_t e, lob_t with, uint32_t at); // seq must be higher than any previous x used with them
void exchange3_free(exchange3_t x);

// simple accessor utilities
uint8_t *exchange3_token(exchange3_t x); // 16 bytes, unique to this exchange for matching/routing
uint32_t exchange3_at(exchange3_t x); // last used at value

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


#endif