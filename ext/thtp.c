#include "ext.h"
#include "switch.h"
#include <string.h>
#include <stdlib.h>

struct thtp_struct 
{
  xht_t index;
  packet_t glob;
};

thtp_t thtp_new(xht_t index)
{
  thtp_t t;
  t = malloc(sizeof (struct thtp_struct));
  memset(t,0,sizeof (struct thtp_struct));
  if(!index) index = xht_new(13);
  t->index = index;
  return t;
}

thtp_t thtp_free(thtp_t t)
{
  if(!t) return t;
  //xht_walk() TODO free all notes
  // free all globs
  xht_free(t->index);
  free(t);
  return NULL;
}

void thtp_glob(thtp_t t, packet_t note)
{
  note->next = t->glob;
  t->glob = note;
}

void thtp_path(thtp_t t, packet_t note)
{
  xht_set(t->index,packet_get_str(note,"path"),(void*)note);
}

packet_t _thtp_glob(thtp_t t, char *p1)
{
  char *p2;
  packet_t cur = t->glob;
  while(cur)
  {
    p2 = packet_get_str(cur,"glob");
    if(strncasecmp(p1,p2,strlen(p2)) == 0) return cur;
    cur = cur->next;
  }
  return NULL;
}

void ext_thtp(thtp_t t, chan_t c)
{
  packet_t p, req, resp, match;
  char *path;

  // incoming note as an answer
  if(c->state == ENDING && (resp = chan_notes(c)))
  {
    // TODO send note as answer
  }

  // use a note to store until the full req
  if(!c->note) c->note = packet_new();

  while((p = chan_pop(c)))
  {
    packet_append(c->note,p->body,p->body_len);
    // for now we're processing whole-requests-at-once, to do streaming we can try parsing note->body for the headers anytime
    if(!c->state == ENDING)
    {
      packet_free(p);
      continue;
    }
    // parse the request if there is one
    if(c->note->body_len > 4)
    {
      packet_free(p);
      req = packet_parse(c->note->body,c->note->body_len);
    }else{
      // use the short version
      req = p;
    }
    path = packet_get_str(req,"path");
    if(!path) return (void)chan_fail(c,"invalid");
    printf("thtp packet %.*s\n", req->json_len, req->json);
    match = xht_get(t->index,path);
    if(!match) match = _thtp_glob(t,path);
    if(!match)
    {
      // TODO send a nice error
      return (void)chan_fail(c,"404");
    }
    resp = chan_packet(c);
    packet_set(resp,"end","true",4);
    packet_body(resp,match->body,match->body_len);
    switch_send(c->s,resp);
  }
  
  // optionally sends ack if needed
  switch_send(c->s,chan_seq_ack(c,NULL));
}