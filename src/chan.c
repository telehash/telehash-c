#include "chan.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "util.h"

chan_t chan_new(switch_t s, hn_t to, char *type, int reliable)
{
  chan_t c;
  c = malloc(sizeof (struct chan_struct));
  bzero(c,sizeof (struct chan_struct));
  c->type = strdup(type);
  c->s = s;
  c->to = to;
  c->reliable = reliable;
  c->state = STARTING;
  crypt_rand(c->id, 16);
  util_hex(c->id,16,(unsigned char*)c->hexid);
  return c;
}

void chan_free(chan_t c)
{
  // remove from hn
  free(c->type);
  free(c);
}

// create a packet ready to be sent for this channel
packet_t chan_packet(chan_t c)
{
  packet_t p = packet_new();
  p->to = c->to;
  if(path_alive(c->last)) p->out = c->last;
  if(c->state == STARTING)
  {
    c->state = OPEN;
    packet_set_str(p,"type",c->type);
  }
  packet_set_str(p,"c",c->hexid);
  return p;
}

// internal, receives/processes incoming packet
void chan_receive(chan_t c, packet_t p)
{
  // check state, drop if ended
  // queue
  // look for handler to fire, else add switch->in
}

// returns any packet to be processed, or NULL
packet_t chan_pop(chan_t c)
{
  // only trigger free after ended and no packets
  return NULL;
}