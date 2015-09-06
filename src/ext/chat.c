#include "telehash.h"

// chatr is just per-chat-channel state holder
typedef struct chatr_struct 
{
  chat_t chat;
  lob_t in;
  int joined;
  int online;
} *chatr_t;

chatr_t chatr_new(chat_t chat)
{
  chatr_t r;
  r = malloc(sizeof (struct chatr_struct));
  memset(r,0,sizeof (struct chatr_struct));
  r->chat = chat;
  r->in = lob_new();
  return r;
}

void chatr_free(chatr_t r)
{
  lob_free(r->in);
  free(r);
}

// validate endpoint part of an id
int chat_eplen(char *id)
{
  int at;
  if(!id) return -1;
  for(at=0;id[at];at++)
  {
    if(id[at] == '@') return at;
    if(id[at] >= 'a' && id[at] <= 'z') continue;
    if(id[at] >= '0' && id[at] <= '9') continue;
    if(id[at] == '_') continue;
    return -1;
  }
  return 0;
}

char *chat_rhash(chat_t chat)
{
  char *buf, *str;
  int at=0, i, len;
  // TODO, should be a way to optimize doing the mmh on the fly
  buf = malloc(chat->roster->json_len);
  lob_sort(chat->roster);
  for(i=0;(str = lob_get_istr(chat->roster,i));i++)
  {
    len = strlen(str);
    memcpy(buf+at,str,len);
    at += len;
  }
  util_murmur((unsigned char*)buf,at,chat->rhash);
  return chat->rhash;
}

// if id is NULL it requests the roster, otherwise requests message id
void chat_cache(chat_t chat, char *hn, char *id)
{
  char *via;
  lob_t note;
  if(!chat || (id && !strchr(id,','))) return;
  // only fetch from origin until joined
  via = (!hn || !chat->join) ? chat->origin->hexname : hn;
  if(id && xht_get(chat->log,id)) return; // cached message id
  if(util_cmp(via,chat->s->id->hexname) == 0) return; // can't request from ourselves
  note = chan_note(chat->hub,NULL);
  lob_set_str(note,"to",via);
  lob_set_str(note,"for",hn);
  if(!id) lob_set_printf(note,"path","/chat/%s/roster",chat->idhash);
  else lob_set_printf(note,"path","/chat/%s/id/%s",chat->idhash,id);
  thtp_req(chat->s,note);  
}

chat_t chat_get(switch_t s, char *id)
{
  chat_t chat;
  lob_t note;
  hashname_t origin = NULL;
  int at;
  char buf[128];

  chat = xht_get(s->index,id);
  if(chat) return chat;

  // if there's an id, validate and optionally parse out originator
  if(id)
  {
    at = chat_eplen(id);
    if(at < 0) return NULL;
    if(at > 0)
    {
      id[at] = 0;
      origin = hashname_gethex(s->index,id+(at+1));
      if(!origin) return NULL;
    }
  }

  chat = malloc(sizeof (struct chat_struct));
  memset(chat,0,sizeof (struct chat_struct));
  if(!id)
  {
    crypt_rand((unsigned char*)buf,4);
    util_hex((unsigned char*)buf,4,(unsigned char*)chat->ep);
  }else{
    memcpy(chat->ep,id,strlen(id)+1);
  }
  chat->origin = origin ? origin : s->id;
  if(chat->origin == s->id) chat->local = 1;
  sprintf(chat->id,"%s@%s",chat->ep,chat->origin->hexname);
  util_murmur((unsigned char*)chat->id,strlen(chat->id),chat->idhash);
  chat->s = s;
  chat->roster = lob_new();
  lob_json(chat->roster,(unsigned char*)"{}",2);
  chat->log = xht_new(101);
  chat->conn = xht_new(7);
  chat->seq = 1000;
  crypt_rand((unsigned char*)&(chat->seed),4);

  // an admin channel for distribution and thtp requests
  chat->hub = chan_new(s, s->id, "chat", 0);
  chat->hub->arg = chatr_new(chat); // stub so we know it's the hub chat channel
  note = chan_note(chat->hub,NULL);
  lob_set_printf(note,"glob","/chat/%s/",chat->idhash);
  DEBUG_PRINTF("chat %s glob %s",chat->id,lob_get_str(note,"glob"));
  thtp_glob(s,0,note);
  xht_set(s->index,chat->id,chat);

  // any other hashname and we try to initialize
  if(!chat->local) chat_cache(chat,chat->origin->hexname,NULL);

  return chat;
}

chat_t chat_free(chat_t chat)
{
  if(!chat) return chat;
  xht_set(chat->s->index,chat->id,NULL);
  // TODO xht-walk chat->log and free packets
  // TODO xht-walk chat->conn and end channels
  // TODO unregister thtp
  // TODO free chat->msgs chain
  xht_free(chat->log);
  xht_free(chat->conn);
  lob_free(chat->roster);
  free(chat);
  return NULL;
}

lob_t chat_message(chat_t chat)
{
  lob_t p;
  char id[32];
  unsigned char buf[4];
  uint16_t step;
  unsigned long at = util_sys_seconds();

  if(!chat || !chat->seq) return NULL;

  p = lob_new();
  // this can be optimized by not converting to/from hex once mmh32 is endian safe
  util_hex((unsigned char*)chat->seed,4,(unsigned char*)id);
  for(step=0; step <= chat->seq; step++) util_murmur(util_unhex((unsigned char*)id,8,(unsigned char*)buf),4,id);
  sprintf(id+8,",%d",chat->seq);
  chat->seq--;
  lob_set_str(p,"id",id);
  lob_set_str(p,"type","chat");
  if(chat->after) lob_set_str(p,"after",chat->after);
  if(at > 1396184861) lob_set_int(p,"at",at); // only if util_sys_seconds() is epoch
  return p;
}

// add msg to queue
void chat_push(chat_t chat, lob_t msg)
{
  lob_t prev;
  if(!chat) return (void)lob_free(msg);
  msg->next = NULL; // paranoid safety
  if(!chat->msgs) return (void)(chat->msgs = msg);
  prev = chat->msgs;
  while(prev->next) prev = prev->next;
  prev->next = msg;
}

lob_t chat_pop(chat_t chat)
{
  lob_t msg;
  if(!chat || !chat->msgs) return NULL;
  msg = chat->msgs;
  chat->msgs = msg->next;
  msg->next = NULL;
  return msg;
}

// updates current stored state to notify app of changes
void chat_restate(chat_t chat, char *hn)
{
  char *id;
  lob_t join, state, cur;
  chan_t c;
  chatr_t r;

  if(!chat || !hn) return;

  // load from roster
  id = lob_get_str(chat->roster,hn);
  if(!id) return;

  // see if there's a join message cached
  join = xht_get(chat->log,id);
  if(join)
  {
    state = lob_copy(join);
  }else{
    state = lob_new();
    if(strchr(id,','))
    {
      // we should have the join id, try to get it again
      chat_cache(chat,hn,id);
      lob_set_str(state,"text","connecting");
    }else{
      lob_set_str(state,"text",id);
    }
  }

  // make a state packet
  lob_set_str(state,"type","state");
  lob_set_str(state,"from",hn);

  if((c = xht_get(chat->conn,hn)) && (r = c->arg) && r->online) lob_set(state,"online","true",4);
  else lob_set(state,"online","false",5);

  // if the new state is the same, drop it
  cur = xht_get(chat->log,hn);
  if(lob_cmp(state,cur) == 0) return (void)lob_free(state);
  
  // replace/save new state
  xht_set(chat->log,lob_get_str(state,"from"),state);
  lob_free(cur);

  // notify if not ourselves
  if(util_cmp(hn,chat->s->id->hexname)) chat_push(chat,lob_copy(state));
}

// process roster to check connection/join state
void chat_sync(chat_t chat)
{
  char *part;
  chan_t c;
  chatr_t r;
  lob_t p;
  int joined, i = 0;

  joined = chat->join ? 1 : 0;
  while((part = lob_get_istr(chat->roster,i)))
  {
    i += 2;
    if(strlen(part) != 64) continue;
    if(util_cmp(part,chat->s->id->hexname) == 0) continue;
    if((c = xht_get(chat->conn,part)) && (r = c->arg) && r->joined == joined) continue;

    DEBUG_PRINTF("initiating chat to %s for %s",part,chat->id);
    // state change
    chat_restate(chat,part);

    // try to connect
    c = chan_start(chat->s, part, "chat");
    if(!c) continue;
    xht_set(chat->conn,c->to->hexname,c);
    r = c->arg = chatr_new(chat);
    r->joined = joined;
    p = chan_packet(c);
    lob_set_str(p,"to",chat->id);
    lob_set_str(p,"from",chat->join);
    lob_set_str(p,"roster",chat->rhash);
    chan_send(c,p);
  }
}

// just add to the log
void chat_log(chat_t chat, lob_t msg)
{
  lob_t cur;
  char *id = lob_get_str(msg,"id");
  if(!chat || !id) return (void)lob_free(msg);
  cur = xht_get(chat->log,id);
  xht_set(chat->log,id,msg);
  lob_free(cur);
}

chat_t chat_join(chat_t chat, lob_t join)
{
  if(!chat || !join) return NULL;

  // paranoid, don't double-join
  if(util_cmp(chat->join,lob_get_str(join,"id")) == 0) return (chat_t)lob_free(join);

  lob_set_str(join,"type","join");
  chat->join = lob_get_str(join,"id");
  chat_log(chat,join);
  lob_set_str(chat->roster,chat->s->id->hexname,chat->join);
  chat_restate(chat,chat->s->id->hexname);
  chat_rhash(chat);
  
  // create/activate all chat channels
  chat_sync(chat);

  return chat;
}

// chunk the packet out
void chat_chunk(chan_t c, lob_t msg)
{
  lob_t chunk;
  unsigned char *raw;
  unsigned short len, space;
  if(!c || !msg) return;
  raw = lob_raw(msg);
  len = lob_len(msg);
  while(len)
  {
    chunk = chan_packet(c);
    if(!chunk) return; // TODO backpressure
    space = lob_space(chunk);
    if(space > len) space = len;
    lob_body(chunk,raw,space);
    if(len==space) lob_set(chunk,"done","true",4);
    chan_send(c,chunk);
    raw+=space;
    len-=space;
  }
}

chat_t chat_send(chat_t chat, lob_t msg)
{
  char *part;
  chan_t c;
  chatr_t r;
  int i = 0;

  if(!chat || !msg) return NULL;

  chat_log(chat,msg);

  // send as a note to all connected
  while((part = lob_get_istr(chat->roster,i)))
  {
    i += 2;
    if(!(c = xht_get(chat->conn,part))) continue;
    r = c->arg;
    if(!r || !r->joined || !r->online) continue;
    chat_chunk(c,msg);
  }
  return NULL;
}

chat_t chat_add(chat_t chat, char *hn, char *val)
{
  if(!chat || !hn || !val) return NULL;
  if(!chat->local) return NULL; // has to be our own to add
  lob_set_str(chat->roster,hn,val);
  chat_rhash(chat);
  chat_sync(chat);
  // try to load if it's a message id
  if(strchr(val,',')) chat_cache(chat,hn,val);
  return chat;
}

// participant permissions, -1 blocked, 0 read-only, 1 allowed
int chat_perm(chat_t chat, char *hn)
{
  char *val;
  if(!chat || !hn) return -1;
  val = lob_get_str(chat->roster,hn);
  if(!val) val = lob_get_str(chat->roster,"*");
  if(!val) return 0;
  if(util_cmp(val,"blocked") == 0) return -1;
  return 1;
}

// this is the hub channel, it just receives notes
chat_t chat_hub(chat_t chat)
{
  lob_t p, note, req, resp;
  char *path, *thtp, *id;
  
  while((note = chan_notes(chat->hub)))
  {
    thtp = lob_get_str(note,"thtp");

    // incoming requests
    if(util_cmp(thtp,"req") == 0)
    {
      req = lob_linked(note);
      DEBUG_PRINTF("note req packet %.*s", req->json_len, req->json);
      path = lob_get_str(req,"path");
      p = lob_new();
      lob_set_int(p,"status",404);
      if(path && strstr(path,"/roster"))
      {
        lob_set_int(p,"status",200);
        lob_body(p,chat->roster->json,chat->roster->json_len);
        DEBUG_PRINTF("roster '%.*s' %d",p->body_len,p->body,lob_len(p));
      }
      if(path && (id = strstr(path,"/id/")) && (id += 4) && (resp = xht_get(chat->log,id)))
      {
        lob_set_int(p,"status",200);
        lob_body(p,lob_raw(resp),lob_len(resp));
      }
      lob_link(note,p);
      chan_reply(chat->hub,note);
    }

    // answers to our requests
    if(util_cmp(thtp,"resp") == 0)
    {
      resp = lob_linked(note);
      path = lob_get_str(note,"path");
      DEBUG_PRINTF("note resp packet %s %.*s", path, resp->json_len, resp->json);
      if(strstr(path,"/roster"))
      {
        p = lob_new();
        if(lob_json(p,resp->body,resp->body_len) == 0)
        {
          lob_free(chat->roster);
          chat->roster = p;
          chat_sync(chat);
        }else{
          lob_free(p);
        }
      }
      if(strstr(path,"/id/"))
      {
        p = lob_parse(resp->body,resp->body_len);
        if(p)
        {
          id = lob_get_str(note,"for");
          lob_set_str(p,"from",id);
          chat_log(chat,p);
          // is either a join to be processed or an old msg
          if(util_cmp(lob_get_str(p,"type"),"join") == 0) chat_restate(chat,id);
          else chat_push(chat,lob_copy(p));
        }
      }
      lob_free(note);
    }
  }

  return chat->msgs ? chat : NULL;
}

chat_t ext_chat(chan_t c)
{
  lob_t p, msg;
  chatr_t r = c->arg;
  chat_t chat = NULL;
  int perm;
  char *id;

  // this is the hub channel, process it there
  if(r && r->chat->hub == c) return chat_hub(r->chat);

  // channel start request
  if(!r)
  {
    if(!(p = chan_pop(c))) return (chat_t)chan_fail(c,"500");
    chat = chat_get(c->s,lob_get_str(p,"to"));
    if(!chat) return (chat_t)chan_fail(c,"500");
    perm = chat_perm(chat,c->to->hexname);
    id = lob_get_str(p,"from");
    DEBUG_PRINTF("chat %s from %s is %d",chat->id,c->to->hexname,perm);
    if(perm < 0) return (chat_t)chan_fail(c,"blocked");
    if(perm == 0 && id) return (chat_t)chan_fail(c,"read-only");

    // legit new chat conn
    r = c->arg = chatr_new(chat);
    r->online = 1;
    xht_set(chat->conn,c->to->hexname,c);

    // response
    p = chan_packet(c);
    if(chat->join)
    {
      r->joined = 1;
      lob_set_str(p,"from",chat->join);
    }

    // add to roster if given
    if(id) chat_add(chat,c->to->hexname,id);
    
    // re-fetch roster if hashes don't match
    if(util_cmp(lob_get_str(p,"roster"),chat->rhash) != 0) chat_cache(chat,chat->origin->hexname,NULL);
    lob_set_str(p,"roster",chat->rhash);

    chan_send(c,p);
  }
  
  // response to a join
  if(!r->online && (p = chan_pop(c)))
  {
    id = lob_get_str(p,"from");
    DEBUG_PRINTF("chat online %s from %s %s",r->chat->id,c->to->hexname,id?id:lob_get_str(p,"err"));
    if(!id) return (chat_t)chan_fail(c,"invalid");
    r->online = 1;
    chat_restate(r->chat,c->to->hexname);
    lob_free(p);
  }

  while((p = chan_pop(c)))
  {
    DEBUG_PRINTF("chat packet %.*s", p->json_len, p->json);
    lob_append(r->in,p->body,p->body_len);
    if(util_cmp(lob_get_str(p,"done"),"true") == 0)
    {
      msg = lob_parse(r->in->body,r->in->body_len);
      if(msg)
      {
        lob_set_str(msg,"from",c->to->hexname);
        chat_push(r->chat,msg);        
      }
      lob_body(r->in,NULL,0);
    }
    lob_free(p);
  }
  
  // optionally sends ack if needed
  chan_ack(c);
  
  if(c->ended)
  {
    // if it's this channel in the index, zap it
    if(xht_get(r->chat->conn,c->to->hexname) == c) xht_set(r->chat->conn,c->to->hexname,NULL);
    chatr_free(r);
    c->arg = NULL;
    return NULL;
  } else {
    return r->chat->msgs ? r->chat : NULL;
  }
}

lob_t chat_participant(chat_t chat, char *hn)
{
  if(!chat) return NULL;
  return xht_get(chat->log,hn);
}

lob_t chat_iparticipant(chat_t chat, int index)
{
  if(!chat) return NULL;
  return xht_get(chat->log,lob_get_istr(chat->roster,index*2));
}
