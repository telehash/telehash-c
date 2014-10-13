#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "exchange3.h"
#include "platform.h"

// make a new exchange
// packet must contain the keys or a handshake to exchange with
exchange3_t exchange3_new(self3_t e, lob_t with, uint32_t at)
{
  exchange3_t x;

  if(!(x = malloc(sizeof (struct exchange3_struct)))) return NULL;
  memset(x,0,sizeof (struct exchange3_struct));

  // TODO create remote

  return x;
}

void exchange3_free(exchange3_t x)
{
  if(!x) return;
  x->cs->remote_free(x->remote);
  free(x);
}

// simple accessor utilities
uint8_t *exchange3_token(exchange3_t x)
{
  if(!x) return NULL;
  return x->token;
}

// last used at value
uint32_t exchange3_at(exchange3_t x)
{
  if(!x) return 0;
  return x->at;
  return 0;
}

// these require a self (local) and an exchange (remote) but are exchange independent
// will safely set/increment seq if 0
lob_t exchange3_message(exchange3_t x, lob_t inner, uint32_t at)
{
  return NULL;
}

// any handshake verify fail (lower seq), always resend handshake
uint8_t exchange3_verify(exchange3_t x, lob_t message)
{
  return 0;
}

// returns the seq value for a handshake reply if needed
// sets secrets/seq/cids to the given handshake if it's newer
// always call chan_sync(c,true) after this on all open channels to signal them the exchange is active
uint32_t exchange3_sync(exchange3_t x, lob_t handshake)
{
  return 0;
}

// just a convenience, seq=0 means force new handshake (and call chan_sync(false)), or seq = exchange3_seq() or exchange3_sync()
lob_t exchange3_handshake(exchange3_t x, lob_t inner, uint32_t seq)
{
  return NULL;
}

// simple encrypt/decrypt conversion of any packet for channels
// goes to channel, validates cid
lob_t exchange3_receive(exchange3_t x, lob_t packet)
{
  return NULL;
}

// comes from channel 
lob_t exchange3_send(exchange3_t x, lob_t inner)
{
  return NULL;
}

// get next avail outgoing channel id
uint32_t exchange3_cid(exchange3_t x)
{
  return 0;
}

