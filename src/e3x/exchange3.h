#ifndef exchange3_h
#define exchange3_h

#include <stdint.h>
#include "cipher3.h"
#include "self3.h"

// apps should only use accessor functions for values in this struct
typedef struct exchange3_struct
{
  cipher3_t cs; // convenience
  self3_t self;
  uint8_t csid, order;
  char hex[3];
  remote_t remote;
  ephemeral_t ephem;
  uint8_t token[16], eid[16];
  uint32_t in, out;
  uint32_t cid;
} *exchange3_t;

// make a new exchange
// packet must contain the raw key in the body
exchange3_t exchange3_new(self3_t self, uint8_t csid, lob_t key);
void exchange3_free(exchange3_t x);

// these are stateless async encryption and verification
lob_t exchange3_message(exchange3_t x, lob_t inner);
uint8_t exchange3_verify(exchange3_t x, lob_t outer);

// return the current incoming at value, optional arg to update it
uint32_t exchange3_in(exchange3_t x, uint32_t at);

// will return the current outgoing at value, optional arg to update it
uint32_t exchange3_out(exchange3_t x, uint32_t at);

// synchronize to incoming ephemeral key and set out at = in at, returns x if success, NULL if not
exchange3_t exchange3_sync(exchange3_t x, lob_t outer);

// generates handshake w/ current exchange3_out value and ephemeral key
lob_t exchange3_handshake(exchange3_t x);

// simple synchronous encrypt/decrypt conversion of any packet for channels
lob_t exchange3_receive(exchange3_t x, lob_t outer); // goes to channel, validates cid
lob_t exchange3_send(exchange3_t x, lob_t inner); // comes from channel 

// get next avail outgoing channel id
uint32_t exchange3_cid(exchange3_t x);

// get the 16-byte token value to this exchange
uint8_t *exchange3_token(exchange3_t x);

#endif