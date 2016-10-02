#ifndef gossip_h
#define gossip_h
#include <stdint.h>
#include "telehash.h"

// any sig/stream frame that contains gossip will be decoded and a list of gossip will be returned
// gossip must be passed back into mesh to be re-distributed afterwards
// 
// gossip creation utilities, pass list in when generating a signal
// ask mesh/link for any additional gossip to fill

typedef struct gossip_s
{
  // the context of the gossip
  link_t sender;
  link_t to; // create link_self check
  hashname_t about; // nickname only
  struct gossip_s *next;

  // the binary 5-byte wire format
  union {
    struct {
      uint8_t type:3; // topic
      uint8_t head:4; // per-topic
      uint8_t done:1; // flag in cur context to end a sequence
      uint32_t body; // payload
    };
    uint8_t bin[5];
  };
  
} gossip_s, *gossip_t;

gossip_t gossip_new(void);
gossip_t gossip_free(gossip_t g);

///////////// stub notes
// 
// tempo_t
// * secret
// * sequence
// - utilities to create, advance, cipher
// - can handle nonce-encoding mode vs. transport sync
// - stable tempos must have way to ensure sequence advances, is never re-used

#endif