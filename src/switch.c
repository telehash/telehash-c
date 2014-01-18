#include "switch.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "j0g.h"

// a prime number for the internal hashtable used to track all active hashnames/lines
#define MAXPRIME 4211

switch_t switch_new(hn_t id)
{
  switch_t s = malloc(sizeof (struct switch_struct));
  bzero(s, sizeof(struct switch_struct));
  s->id = id;
  s->cap = 256; // default cap size
  // create all the buckets
  s->buckets = malloc(256 * sizeof(bucket_t));
  bzero(s->buckets, 256 * sizeof(bucket_t));
  s->index = xht_new(MAXPRIME);
  return s;
}

int switch_init(switch_t s, hn_t id)
{
  if(!id || !id->c) return 1;
  if(crypt_private(id->c,NULL,0)) return 1;
  s->id = id;
  return 0;
}

void switch_free(switch_t s)
{
  int i;
  for(i=0;i<=255;i++) if(s->buckets[i]) bucket_free(s->buckets[i]);
  if(s->seeds) bucket_free(s->seeds);
  free(s);
}

void switch_cap(switch_t s, int cap)
{
  s->cap = cap;
}

// add this hashname to our bucket list
void switch_bucket(switch_t s, hn_t hn)
{
  unsigned char bucket = hn_distance(s->id, hn);
  if(!s->buckets[bucket]) s->buckets[bucket] = bucket_new();
  bucket_add(s->buckets[bucket], hn);
  // TODO figure out if there's actually more capacity
  
}

void switch_seed(switch_t s, hn_t hn)
{
  if(!s->seeds) s->seeds = bucket_new();
  bucket_add(s->seeds, hn);
}

packet_t switch_sending(switch_t s)
{
  packet_t p;
  if(!s || !s->out) return NULL;
  p = s->out;
  s->out = p->next;
  if(!s->out) s->last = NULL;
  return p;
}

// internally adds to sending queue
void switch_sendingQ(switch_t s, packet_t p)
{
  packet_t dup;
  if(!p) return;

  // if there's no path, find one or copy to many
  if(!p->out)
  {
    // just being paranoid
    if(!p->to)
    {
      packet_free(p);
      return;
    }

    // if the last path is alive, just use that
    if(path_alive(p->to->last)) p->out = p->to->last;
    else{
      int i;
      // try sending to all paths
      for(i=0; p->to->paths[i]; i++)
      {
        dup = packet_copy(p);
        dup->out = p->to->paths[i];
        switch_sendingQ(s, dup);
      }
      packet_free(p);
      return;   
    }
  }
  
  // update stats
  p->out->atOut = time(0);

  // add to the end of the queue
  if(s->last)
  {
    s->last->next = p;
    s->last = p;
    return;
  }
  s->last = s->out = p;  
}

// tries to send an open if we haven't
void switch_open(switch_t s, hn_t to, path_t direct)
{
  packet_t open;
  time_t now;
  
  if(!to) return;

  // don't send too frequently
  now = time(0);
  if(now - to->sentOpen < 2) return;
  to->sentOpen = now;

  // actually send the open
  open = crypt_openize(s->id->c, to->c);
  if(!open) return;
  open->to = to;
  if(direct) open->out = direct;
  switch_sendingQ(s, open);
}

void switch_send(switch_t s, packet_t p)
{
  packet_t lined;

  if(!p) return;
  
  // require recipient at least
  if(!p->to) return (void)packet_free(p);

  // encrypt the packet to the line, chains together
  lined = crypt_lineize(s->id->c, p->to->c, p);
  if(lined) return switch_sendingQ(s, lined);

  // queue most recent packet to be sent after opened
  if(p->to->onopen) packet_free(p->to->onopen);
  p->to->onopen = p;

  // no line, so generate open instead
  switch_open(s, p->to, NULL);
}

chan_t switch_pop(switch_t s)
{
  chan_t c;
  if(!s->chans) return NULL;
  c = s->chans;
  s->chans = c->next;
  return c;
}

void switch_receive(switch_t s, packet_t p, path_t in)
{
  hn_t from;
  packet_t line;
  crypt_t opened;

  if(!s || !p || !in) return;
  if(strcmp("open",j0g_str("type",(char*)p->json,p->js)) == 0)
  {
    opened = crypt_deopenize(s->id->c, p);
    if(opened)
    {
      from = hn_get(s->index, crypt_hashname(opened));
      from->c = crypt_merge(from->c, opened);
      switch_open(s, from, NULL); // in case we need to send an open
      xht_set(s->index, crypt_line(from->c), from);
      in = hn_path(from, in);
      if(from->onopen)
      {
        packet_t last = from->onopen;
        from->onopen = NULL;
        last->out = in;
        switch_send(s, last);
      }
    }
    return;
  }
  if(strcmp("line",j0g_str("type",(char*)p->json,p->js)) == 0)
  {
    from = xht_get(s->index, packet_get_str(p, "line"));
    if(from)
    {
      in = hn_path(from, in);
      line = crypt_delineize(s->id->c, from->c, p);
      if(line)
      {
        chan_t c = hn_chan(from, packet_get_str(p, "c"), NULL);
        if(!c) c = chan_new(s, from, packet_get_str(p, "type"), packet_get_str(p,"seq")?1:0);
        if(!c) return (void)packet_free(line);
        chan_receive(c, line);
      }
      return;
    }
  }
  
  // nothing processed, clean up
  packet_free(p);
  path_free(in);
}

