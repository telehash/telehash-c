#ifndef ext_chat_h
#define ext_chat_h

/*
#include "switch.h"

typedef struct chat_struct 
{
  char ep[32+1], id[32+1+64+1], idhash[9];
  hashname_t origin;
  switch_t s;
  chan_t hub;
  char rhash[9];
  uint8_t local, seed[4];
  uint16_t seq;
  lob_t roster;
  xht_t conn, log;
  lob_t msgs;
  char *join, *sent, *after;
} *chat_t;

chat_t ext_chat(chan_t c);

chat_t chat_get(switch_t s, char *id);
chat_t chat_free(chat_t chat);

// get the next incoming message (type state/message), caller must free
lob_t chat_pop(chat_t chat);

lob_t chat_message(chat_t chat);
chat_t chat_join(chat_t chat, lob_t join);
chat_t chat_send(chat_t chat, lob_t msg);
chat_t chat_add(chat_t chat, char *hn, char *val);

// get a participant or walk the list, returns the current state packet (immutable), online:true/false
lob_t chat_participant(chat_t chat, char *hn);
lob_t chat_iparticipant(chat_t chat, int index);

*/
#endif
