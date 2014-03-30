#ifndef ext_chat_h
#define ext_chat_h

#include "switch.h"

typedef struct chat_struct 
{
  enum {LOADING, OFFLINE, CONNECTING, CONNECTED, JOINING, JOINED} state;
  char ep[32+1], id[32+1+64+1];
  hn_t orig;
  switch_t s;
  chan_t base;
  char seed[9], rhash[9];
  uint16_t seq;
  packet_t roster;
  xht_t index;
  char *join, *sent, *after;
} *chat_t;

chat_t ext_chat(chan_t c);

chat_t chat_get(switch_t s, char *id);
chat_t chat_free(chat_t ct);

packet_t chat_message(chat_t ct);
chat_t chat_join(chat_t ct, packet_t join);
chat_t chat_send(chat_t ct, packet_t msg);

#endif