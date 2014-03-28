#include "ext.h"

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
  packet_t p, req, resp, match, note;
  char *path;

  // incoming note as an answer
  if(c->state == ENDING && (note = chan_notes(c)))
  {
    printf("got note resp %.*s",note->json_len,note->json);
    resp = chan_packet(c);
    packet_set(resp,"end","true",4);
    packet_body(resp,note->body,note->body_len);
    chan_send(c,resp);
    packet_free(note);
    return;
  }

  while((p = chan_pop(c)))
  {
    if(!c->arg)
    {
      c->arg = req = p;
    }else{
      req = c->arg;
      packet_append(req,p->body,p->body_len);
      packet_free(p);
    }
    // for now we're processing whole-requests-at-once, to do streaming we can try parsing note->body for the headers anytime
    if(!c->state == ENDING) continue;
    // parse the request if there is one
    if(req->body_len > 4)
    {
      p = packet_parse(req->body,req->body_len);
      if(p)
      {
        packet_free(req);
        req = p;
      }
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
    // built in response
    if(match->body_len)
    {
      resp = chan_packet(c);
      packet_set(resp,"end","true",4);
      packet_body(resp,match->body,match->body_len);
      chan_send(c,resp);
      return;
    }
    
    // attach and route request to a new note
    note = packet_copy(match);
    packet_link(note,req);
    if(chan_reply(c,note) != 0) return (void)chan_fail(c,"500");
  }
  
  // optionally sends ack if needed
  chan_ack(c);
}