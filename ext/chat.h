#ifndef ext_chat_h
#define ext_chat_h

#include "switch.h"
#include "thtp.h"

typedef struct chat_struct *chat_t;

chat_t ext_chat(chan_t c);

chat_t chat_get(switch_t s, thtp_t t, char *id);
chat_t chat_free(chat_t ct);

#endif