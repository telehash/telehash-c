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
} *chat_t;

chat_t ext_chat(chan_t c);

chat_t chat_get(switch_t s, thtp_t t, char *id);
chat_t chat_free(chat_t ct);

#endif