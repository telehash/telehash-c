#ifndef ext_chat_h
#define ext_chat_h

#include "switch.h"
#include "thtp.h"

typedef struct chat_struct 
{
  char name[32+1], id[32+1+64+1];
  hn_t orig;
  switch_t s;
  thtp_t t;
  chan_t base;
  char seed[9];
  uint16_t seq;
  packet_t join;
} *chat_t;

chat_t ext_chat(chan_t c);

chat_t chat_get(switch_t s, thtp_t t, char *id);
chat_t chat_free(chat_t ct);

packet_t chat_join(chat_t ct, uint16_t count);

#endif