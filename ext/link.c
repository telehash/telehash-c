#include "ext.h"

#define LINK_TPING 29
#define LINK_TOUT 65

typedef struct link_struct 
{
  // only used when meshing
  int meshing;
  bucket_t meshed;
  // only used when seeding
  int seeding;
  bucket_t *buckets;
  xht_t links;
} *link_t;

link_t link_new(switch_t s)
{
  link_t l;
  l = malloc(sizeof (struct link_struct));
  memset(l,0,sizeof (struct link_struct));
  xht_set(s->index,"link",l);
  return l;
}

link_t link_get(switch_t s)
{
  link_t l;
  l = xht_get(s->index,"link");
  return l ? l : link_new(s);
}

void link_free(switch_t s)
{
  int i;
  link_t l = link_get(s);
  if(l->seeding)
  {
    for(i=0;i<=255;i++) if(l->buckets[i]) bucket_free(l->buckets[i]);
    xht_free(l->links);
  }
  if(l->meshing)
  {
    bucket_free(l->meshed);
  }
  free(l);
}

// automatically mesh any links
void link_mesh(switch_t s, int max)
{
  link_t l = link_get(s);
  l->meshing = max;
  if(!max) return;
  if(!l->meshed) l->meshed = bucket_new();
  // TODO check s->seeds
}

// enable acting as a seed
void link_seed(switch_t s, int max)
{
  link_t l = link_get(s);
  l->seeding = max;
  if(!max) return;
  // create all the buckets
  l->buckets = malloc(256 * sizeof(bucket_t));
  memset(l->buckets, 0, 256 * sizeof(bucket_t));
  l->links = xht_new(max);
}

/*
  unsigned char bucket = hn_distance(s->id, hn);
  if(!s->buckets[bucket]) s->buckets[bucket] = bucket_new();
  bucket_add(s->buckets[bucket], hn);
  // TODO figure out if there's actually more capacity
*/

// adds regular ping data and sends
void link_ping(link_t l, chan_t c, packet_t p)
{
  if(!l || !c || !p) return;
  if(l->seeding) packet_set(p,"seed","true",4);
  chan_send(c, p);
}

// create/fetch/maintain a link to this hn, fires note on UP and DOWN change events
chan_t link_hn(switch_t s, hn_t h, packet_t note)
{
  chan_t c;
  link_t l = link_get(s);
  if(!s || !h) return NULL;

  c = chan_new(s, h, "link", 0);
  c->arg = note;
  link_ping(l, c, chan_packet(c));
  return c;
}

void ext_link(chan_t c)
{
  packet_t p, note = (packet_t)c->arg;
  link_t l = link_get(c->s);

  while((p = chan_pop(c)))
  {
    DEBUG_PRINTF("TODO link packet %.*s\n", p->json_len, p->json);      
    packet_free(p);
  }
  // respond/ack if we haven't recently
  if(c->s->tick - c->tsent > (LINK_TPING/2)) link_ping(l, c, chan_packet(c));

  if(c->state != CHAN_OPEN)
  {
    // TODO bucket eviction
  }
  
  // if no note, nothing else to do
  if(!note) return;

  // handle note UP/DOWN change based on channel state
  if(c->state == CHAN_OPEN)
  {
    if(util_cmp(packet_get_str(note,"link"),"up") != 0)
    {
      packet_set_str(note,"link","up");
      // TODO send note
    }
  }else{
    if(util_cmp(packet_get_str(note,"link"),"down") != 0)
    {
      packet_set_str(note,"link","down");
      // TODO send note
    }
    c->arg = NULL;
    link_hn(c->s,c->to,note);
  }
  
}

void ext_seek(chan_t c)
{
  printf("TODO handle seek channel\n");
}