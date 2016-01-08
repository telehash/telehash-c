#ifndef e3x_exchange_h
#define e3x_exchange_h

#include <stdint.h>
#include "e3x_cipher.h"
#include "e3x_self.h"

// apps should only use accessor functions for values in this struct
typedef struct e3x_exchange_struct
{
  e3x_cipher_t cs; // convenience
  e3x_self_t self;
  uint8_t csid, order;
  char hex[3];
  remote_t remote;
  ephemeral_t ephem;
  uint8_t token[16], eid[16];
  uint32_t in, out;
  uint32_t cid, last;
} *e3x_exchange_t;

// make a new exchange
// packet must contain the raw key in the body
e3x_exchange_t e3x_exchange_new(e3x_self_t self, uint8_t csid, lob_t key);
void e3x_exchange_free(e3x_exchange_t x);

// these are stateless async encryption and verification
lob_t e3x_exchange_message(e3x_exchange_t x, lob_t inner);
uint8_t e3x_exchange_verify(e3x_exchange_t x, lob_t outer);
uint8_t e3x_exchange_validate(e3x_exchange_t x, lob_t args, lob_t sig, uint8_t *data, size_t len);

// return the current incoming at value, optional arg to update it
uint32_t e3x_exchange_in(e3x_exchange_t x, uint32_t at);

// will return the current outgoing at value, optional arg to update it
uint32_t e3x_exchange_out(e3x_exchange_t x, uint32_t at);

// synchronize to incoming ephemeral key and set out at = in at, returns x if success, NULL if not
e3x_exchange_t e3x_exchange_sync(e3x_exchange_t x, lob_t outer);

// drops ephemeral state, out=0
e3x_exchange_t e3x_exchange_down(e3x_exchange_t x);

// generates handshake w/ current e3x_exchange_out value and ephemeral key
lob_t e3x_exchange_handshake(e3x_exchange_t x, lob_t inner);

// simple synchronous encrypt/decrypt conversion of any packet for channels
lob_t e3x_exchange_receive(e3x_exchange_t x, lob_t outer); // goes to channel, validates cid
lob_t e3x_exchange_send(e3x_exchange_t x, lob_t inner); // comes from channel 

// validate the next incoming channel id from the packet, or return the next avail outgoing channel id
uint32_t e3x_exchange_cid(e3x_exchange_t x, lob_t incoming);

// get the 16-byte token value to this exchange
uint8_t *e3x_exchange_token(e3x_exchange_t x);

#endif
