#include "ext.h"

typedef struct thtp_struct 
{
  xht_t index;
  packet_t glob;
} *thtp_t;

thtp_t thtp_new(switch_t s, xht_t index)
{
  thtp_t t;
  t = malloc(sizeof (struct thtp_struct));
  memset(t,0,sizeof (struct thtp_struct));
  if(!index) index = xht_new(13);
  t->index = index;
  xht_set(s->index,"thtp",t);
  return t;
  
}

// typeless wrapper
void thtp_init(switch_t s, xht_t index)
{
  thtp_new(s,index);
}

thtp_t thtp_get(switch_t s)
{
  thtp_t t;
  t = xht_get(s->index,"thtp");
  return t ? t : thtp_new(s,NULL);
}

void thtp_free(switch_t s)
{
  thtp_t t = thtp_get(s);
  //xht_walk() TODO free all notes
  // free all globs
  xht_free(t->index);
  free(t);
}

// TODO support NULL note to delete
void thtp_glob(switch_t s, char *glob, packet_t note)
{
  thtp_t t = thtp_get(s);
  if(glob) packet_set_str(note,"glob",glob);
  note->next = t->glob;
  t->glob = note;
}

void thtp_path(switch_t s, char *path, packet_t note)
{
  thtp_t t = thtp_get(s);
  packet_set_str(note,"path",path);
  xht_set(t->index,packet_get_str(note,"path"),(void*)note);
}

packet_t _thtp_glob(thtp_t t, char *p1)
{
  char *p2;
  packet_t cur;
  if(!t || !p1) return NULL;
  cur = t->glob;
  while(cur)
  {
    p2 = packet_get_str(cur,"glob");
    if(strncasecmp(p1,p2,strlen(p2)) == 0) return cur;
    cur = cur->next;
  }
  return NULL;
}

// chunk the packet out
void thtp_send(chan_t c, packet_t req)
{
  packet_t chunk;
  unsigned char *raw;
  unsigned short len, space;
  if(!c || !req) return;
  DEBUG_PRINTF("THTP sending %.*s %.*s",req->json_len,req->json,req->body_len,req->body);
  raw = packet_raw(req);
  len = packet_len(req);
  while(len)
  {
    chunk = chan_packet(c);
    if(!chunk) return; // TODO backpressure
    space = packet_space(chunk);
    if(space > len) space = len;
    packet_body(chunk,raw,space);
    if(len==space) packet_set(chunk,"end","true",4);
    chan_send(c,chunk);
    raw+=space;
    len-=space;
  }
}

// generate an outgoing request, send the response attached to the note
chan_t thtp_req(switch_t s, packet_t note)
{
  char *uri, *path, *method;
  hn_t to = NULL;
  packet_t req;
  chan_t c;
  if(!s || !note) return NULL;

  method = packet_get_str(note,"method");
  path = packet_get_str(note,"path");
  if((uri = packet_get_str(note,"uri")) && strncmp(uri,"thtp://",7) == 0)
  {
    uri += 7;
    path = strchr(uri,'/');
    to = hn_gethex(s->index,uri);
  }
  if(!to) to = hn_gethex(s->index,packet_get_str(note,"to"));
  if(!to) return NULL;
  req = packet_linked(note);
  if(!req)
  {
    req = packet_chain(note);
    packet_set_str(req,"path",path?path:"/");
    packet_set_str(req,"method",method?method:"get");
  }

  DEBUG_PRINTF("thtp req %s %s %s %.*s",packet_get_str(req,"method"),packet_get_str(req,"path"),to->hexname,note->json_len,note->json);

  // open channel and send req
  c = chan_new(s, to, "thtp", 0);
  c->arg = packet_link(NULL,note); // create buffer packet w/ the note linked
  c->handler = ext_thtp; // shortcut
  chan_reliable(c,10);
  thtp_send(c,req);

  return c;
}

void ext_thtp(chan_t c)
{
  packet_t p, buf, req, match, note;
  char *path;
  thtp_t t = thtp_get(c->s);

  // incoming note as an answer
  if(c->state == ENDING && (note = chan_notes(c)))
  {
    DEBUG_PRINTF("got note resp %.*s",note->json_len,note->json);
    thtp_send(c,packet_linked(note));
    packet_free(note);
    return;
  }

  while((p = chan_pop(c)))
  {
    if(!c->arg)
    {
      c->arg = buf = p;
    }else{
      buf = c->arg;
      packet_append(buf,p->body,p->body_len);
      packet_free(p);
    }
    // for now we're processing whole-requests-at-once, to do streaming we can try parsing note->body for the headers anytime
    if(!c->state == ENDING) continue;

    // parse the payload
    p = packet_parse(buf->body,buf->body_len);

    // this is a response, send it
    if((note = packet_unlink(buf)))
    {
      packet_free(buf);
      if(p)
      {
        DEBUG_PRINTF("got response %.*s for %.*s",p->json_len,p->json,note->json_len,note->json);        
      }
      packet_link(note,p);
      packet_set_str(note,"thtp","resp");
      chan_reply(c,note);
      chan_end(c,NULL);
      return;
    }

    // this is an incoming request
    packet_free(buf);
    if(!p) return (void)chan_fail(c,"422");
    req = p;

    DEBUG_PRINTF("thtp req packet %.*s", req->json_len, req->json);
    path = packet_get_str(req,"path");
    match = xht_get(t->index,path);
    if(!match) match = _thtp_glob(t,path);
    if(!match)
    {
      chan_fail(c,"404");
      packet_free(req);
      return;
    }

    // built in response
    if(packet_linked(match))
    {
      thtp_send(c,packet_linked(match));
      packet_free(req);
      return;
    }
    
    // attach and route request to a new note
    note = packet_copy(match);
    packet_link(note,req);
    packet_set_str(note,"thtp","req");
    if(chan_reply(c,note) == 0) return;

    chan_fail(c,"500");
    packet_free(req);
  }
  
  // optionally sends ack if needed
  chan_ack(c);
}