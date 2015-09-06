#include "telehash.h"

typedef struct thtp_struct 
{
  xht_t index;
  lob_t glob;
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
void thtp_glob(switch_t s, char *glob, lob_t note)
{
  thtp_t t = thtp_get(s);
  if(glob) lob_set_str(note,"glob",glob);
  note->next = t->glob;
  t->glob = note;
}

void thtp_path(switch_t s, char *path, lob_t note)
{
  thtp_t t = thtp_get(s);
  lob_set_str(note,"path",path);
  xht_set(t->index,lob_get_str(note,"path"),(void*)note);
}

lob_t _thtp_glob(thtp_t t, char *p1)
{
  char *p2;
  lob_t cur;
  if(!t || !p1) return NULL;
  cur = t->glob;
  while(cur)
  {
    p2 = lob_get_str(cur,"glob");
    if(strncasecmp(p1,p2,strlen(p2)) == 0) return cur;
    cur = cur->next;
  }
  return NULL;
}

// chunk the packet out
void thtp_send(chan_t c, lob_t req)
{
  lob_t chunk;
  unsigned char *raw;
  unsigned short len, space;
  if(!c || !req) return;
  DEBUG_PRINTF("THTP sending %.*s %.*s",req->json_len,req->json,req->body_len,req->body);
  raw = lob_raw(req);
  len = lob_len(req);
  while(len)
  {
    chunk = chan_packet(c);
    if(!chunk) return; // TODO backpressure
    space = lob_space(chunk);
    if(space > len) space = len;
    lob_body(chunk,raw,space);
    if(len==space) lob_set(chunk,"end","true",4);
    chan_send(c,chunk);
    raw+=space;
    len-=space;
  }
}

// generate an outgoing request, send the response attached to the note
chan_t thtp_req(switch_t s, lob_t note)
{
  char *uri, *path, *method;
  hashname_t to = NULL;
  lob_t req;
  chan_t c;
  if(!s || !note) return NULL;

  method = lob_get_str(note,"method");
  path = lob_get_str(note,"path");
  if((uri = lob_get_str(note,"uri")) && strncmp(uri,"thtp://",7) == 0)
  {
    uri += 7;
    path = strchr(uri,'/');
    to = hashname_gethex(s->index,uri);
  }
  if(!to) to = hashname_gethex(s->index,lob_get_str(note,"to"));
  if(!to) return NULL;
  req = lob_linked(note);
  if(!req)
  {
    req = lob_chain(note);
    lob_set_str(req,"path",path?path:"/");
    lob_set_str(req,"method",method?method:"get");
  }

  DEBUG_PRINTF("thtp req %s %s %s %.*s",lob_get_str(req,"method"),lob_get_str(req,"path"),to->hexname,note->json_len,note->json);

  // open channel and send req
  c = chan_new(s, to, "thtp", 0);
  c->arg = lob_link(NULL,note); // create buffer packet w/ the note linked
  c->handler = ext_thtp; // shortcut
  chan_reliable(c,10);
  thtp_send(c,req);

  return c;
}

void ext_thtp(chan_t c)
{
  lob_t p, buf, req, match, note;
  char *path;
  thtp_t t = thtp_get(c->s);

  // incoming note as an answer
  if((note = chan_notes(c)))
  {
    DEBUG_PRINTF("got note resp %.*s",note->json_len,note->json);
    thtp_send(c,lob_linked(note));
    lob_free(note);
    return;
  }

  while((p = chan_pop(c)))
  {
    if(!c->arg)
    {
      c->arg = buf = p;
    }else{
      buf = c->arg;
      lob_append(buf,p->body,p->body_len);
      lob_free(p);
    }
    // for now we're processing whole-requests-at-once, to do streaming we can try parsing note->body for the headers anytime
    if(c->ended) continue;

    // parse the payload
    p = lob_parse(buf->body,buf->body_len);

    // this is a response, send it
    if((note = lob_unlink(buf)))
    {
      lob_free(buf);
      if(p)
      {
        DEBUG_PRINTF("got response %.*s for %.*s",p->json_len,p->json,note->json_len,note->json);        
      }
      lob_link(note,p);
      lob_set_str(note,"thtp","resp");
      chan_reply(c,note);
      chan_end(c,NULL);
      return;
    }

    // this is an incoming request
    lob_free(buf);
    if(!p) return (void)chan_fail(c,"422");
    req = p;

    DEBUG_PRINTF("thtp req packet %.*s", req->json_len, req->json);
    path = lob_get_str(req,"path");
    match = xht_get(t->index,path);
    if(!match) match = _thtp_glob(t,path);
    if(!match)
    {
      chan_fail(c,"404");
      lob_free(req);
      return;
    }

    // built in response
    if(lob_linked(match))
    {
      thtp_send(c,lob_linked(match));
      lob_free(req);
      return;
    }
    
    // attach and route request to a new note
    note = lob_copy(match);
    lob_link(note,req);
    lob_set_str(note,"thtp","req");
    if(chan_reply(c,note) == 0) return;

    chan_fail(c,"500");
    lob_free(req);
  }
  
  // optionally sends ack if needed
  chan_ack(c);
}
