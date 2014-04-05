#include "ext.h"

// chatr is just per-chat-channel state holder
typedef struct chatr_struct 
{
  chat_t chat;
  packet_t in;
  int joined;
} *chatr_t;

chatr_t chatr_new(chat_t chat)
{
  chatr_t r;
  r = malloc(sizeof (struct chatr_struct));
  memset(r,0,sizeof (struct chatr_struct));
  r->chat = chat;
  r->in = packet_new();
  return r;
}

void chatr_free(chatr_t r)
{
  packet_free(r->in);
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
  packet_sort(chat->roster);
  for(i=0;(str = packet_get_istr(chat->roster,i));i++)
  {
    len = strlen(str);
    memcpy(buf+at,str,len);
    at += len;
  }
  util_murmur((unsigned char*)buf,at,chat->rhash);
  return chat->rhash;
}

// if id is NULL it requests the roster, otherwise requests message id
void chat_cache(chat_t chat, char *from, char *id)
{
  packet_t note;
  if(!chat || (id && !strchr(id,','))) return;
  if(!from) from = chat->origin->hexname;
  if(id && xht_get(chat->log,id)) return; // cached message id
  if(util_cmp(from,chat->s->id->hexname) == 0) return; // can't request from ourselves
  note = chan_note(chat->hub,NULL);
  packet_set_str(note,"to",from);
  if(!id) packet_set_printf(note,"path","/chat/%s/roster",chat->idhash);
  else packet_set_printf(note,"path","/chat/%s/id/%s",chat->idhash,id);
  thtp_req(chat->s,note);  
}

chat_t chat_get(switch_t s, char *id)
{
  chat_t chat;
  packet_t note;
  hn_t origin = NULL;
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
      origin = hn_gethex(s->index,id+(at+1));
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
  sprintf(chat->id,"%s@%s",chat->ep,chat->origin->hexname);
  util_murmur((unsigned char*)chat->id,strlen(chat->id),chat->idhash);
  chat->s = s;
  chat->roster = packet_new();
  packet_json(chat->roster,(unsigned char*)"{}",2);
  chat->log = xht_new(101);
  chat->conn = xht_new(7);
  chat->seq = 1000;
  crypt_rand((unsigned char*)&(chat->seed),4);

  // an admin channel for distribution and thtp requests
  chat->hub = chan_new(s, s->id, "chat", 0);
  chat->hub->arg = chatr_new(chat); // stub so we know it's the hub chat channel
  note = chan_note(chat->hub,NULL);
  packet_set_printf(note,"glob","/chat/%s/",chat->idhash);
  DEBUG_PRINTF("chat %s glob %s",chat->id,packet_get_str(note,"glob"));
  thtp_glob(s,0,note);
  xht_set(s->index,chat->id,chat);

  // any other hashname and we try to initialize
  if(chat->origin != s->id)
  {
    chat->state = LOADING;
    chat_cache(chat,chat->origin->hexname,NULL);
  }else{
    chat->state = OFFLINE;
  }
  return chat;
}

chat_t chat_free(chat_t chat)
{
  if(!chat) return chat;
  xht_set(chat->s->index,chat->id,NULL);
  // TODO xht-walk chat->log and free packets
  // TODO xht-walk chat->conn and end channels
  // TODO unregister thtp
  xht_free(chat->log);
  xht_free(chat->conn);
  packet_free(chat->roster);
  free(chat);
  return NULL;
}

packet_t chat_message(chat_t chat)
{
  packet_t p;
  char id[32];
  unsigned char buf[4];
  uint16_t step;
  unsigned long at = platform_seconds();

  if(!chat || !chat->seq) return NULL;

  p = packet_new();
  // this can be optimized by not converting to/from hex once mmh32 is endian safe
  util_hex((unsigned char*)chat->seed,4,(unsigned char*)id);
  for(step=0; step <= chat->seq; step++) util_murmur(util_unhex((unsigned char*)id,8,(unsigned char*)buf),4,id);
  sprintf(id+8,",%d",chat->seq);
  chat->seq--;
  packet_set_str(p,"id",id);
  packet_set_str(p,"type","message");
  if(chat->after) packet_set_str(p,"after",chat->after);
  if(at > 1396184861) packet_set_int(p,"at",at); // only if platform_seconds() is epoch
  return p;
}

// process roster to check connection/join state
void chat_sync(chat_t chat)
{
  char *part;
  chan_t c;
  chatr_t r;
  packet_t p;
  int joined, i = 0;

  joined = chat->join ? 1 : 0;
  while((part = packet_get_istr(chat->roster,i++)))
  {
    if((c = xht_get(chat->conn,part)) && (r = c->arg) && r->joined == joined) continue;
    
    // try to connect
    c = chan_start(chat->s, part, "chat");
    if(!c) continue;
    xht_set(chat->conn,c->to->hexname,c);
    r = c->arg = chatr_new(chat);
    r->joined = joined;
    p = chan_packet(c);
    packet_set_str(p,"to",chat->id);
    packet_set_str(p,"from",chat->join);
    packet_set_str(p,"roster",chat->rhash);
    chan_send(c,p);
  }
}

chat_t chat_join(chat_t chat, packet_t join)
{
  packet_t p;

  if(!chat || !join) return NULL;
  packet_set_str(join,"type","join");
  if(chat->join)
  {
    p = xht_get(chat->log,chat->join);
    xht_set(chat->log,chat->join,NULL);
    packet_free(p);
  }
  chat->join = packet_get_str(join,"id");
  xht_set(chat->log,chat->join,join);
  packet_set_str(chat->roster,chat->s->id->hexname,chat->join);
  chat_rhash(chat);
  if(chat->origin == chat->s->id)
  {
    chat->state = JOINED;
  }else{
    chat->state = JOINING;
  }
  
  // create/activate all chat channels
  chat_sync(chat);

  return chat;
}

// chunk the packet out
void chat_chunk(chan_t c, packet_t msg)
{
  packet_t chunk;
  unsigned char *raw;
  unsigned short len, space;
  if(!c || !msg) return;
  raw = packet_raw(msg);
  len = packet_len(msg);
  while(len)
  {
    chunk = chan_packet(c);
    if(!chunk) return; // TODO backpressure
    space = packet_space(chunk);
    if(space > len) space = len;
    packet_body(chunk,raw,space);
    if(len==space) packet_set(chunk,"done","true",4);
    chan_send(c,chunk);
    raw+=space;
    len-=space;
  }
}

chat_t chat_send(chat_t chat, packet_t msg)
{
  char *part;
  chan_t c;
  chatr_t r;
  int i = 0;

  if(!chat || !msg) return NULL;

  // send as a note to all connected
  while((part = packet_get_istr(chat->roster,i++)))
  {
    if(!(c = xht_get(chat->conn,part))) continue;
    r = c->arg;
    if(!r || !r->joined) continue;
    chat_chunk(c,msg);
  }
  return NULL;
}

chat_t chat_add(chat_t chat, char *hn, char *val)
{
  if(!chat || !hn || !val) return NULL;
  packet_set_str(chat->roster,hn,val);
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
  val = packet_get_str(chat->roster,hn);
  if(!val) val = packet_get_str(chat->roster,"*");
  if(!val) return 0;
  if(util_cmp(val,"blocked") == 0) return -1;
  return 1;
}

// this is the hub channel, it just receives notes
chat_t chat_hub(chat_t chat)
{
  packet_t p, note, req, resp;
  char *path, *thtp, *id;
  
  while((note = chan_notes(chat->hub)))
  {
    thtp = packet_get_str(note,"thtp");

    // incoming requests
    if(util_cmp(thtp,"req") == 0)
    {
      req = packet_linked(note);
      printf("note req packet %.*s\n", req->json_len, req->json);
      path = packet_get_str(req,"path");
      p = packet_new();
      packet_set_int(p,"status",404);
      if(path && strstr(path,"/roster"))
      {
        packet_set_int(p,"status",200);
        packet_body(p,chat->roster->json,chat->roster->json_len);
        DEBUG_PRINTF("roster '%.*s' %d",p->body_len,p->body,packet_len(p));
      }
      if(path && (id = strstr(path,"/id/")) && (id += 4) && (resp = xht_get(chat->log,id)))
      {
        packet_set_int(p,"status",200);
        packet_body(p,packet_raw(resp),packet_len(resp));
      }
      packet_link(note,p);
      chan_reply(chat->hub,note);
    }

    // answers to our requests
    if(util_cmp(thtp,"resp") == 0)
    {
      resp = packet_linked(note);
      path = packet_get_str(note,"path");
      printf("note resp packet %s %.*s\n", path, resp->json_len, resp->json);
      // TODO process roster or msg result, roster runs chat_cache for each
      packet_free(note);
    }
  }

  // TODO if the chat is active return it or not
  return chat;
}

chat_t ext_chat(chan_t c)
{
  packet_t p;
  chatr_t r = c->arg;
  chat_t chat;
  int perm;
  char *id;

  // this is the hub channel, process it there
  if(r && r->chat->hub == c) return chat_hub(r->chat);

  // channel start request
  if(!r)
  {
    if(!(p = chan_pop(c))) return (chat_t)chan_fail(c,"500");
    chat = chat_get(c->s,packet_get_str(p,"to"));
    if(!chat) return (chat_t)chan_fail(c,"500");
    perm = chat_perm(chat,c->to->hexname);
    id = packet_get_str(p,"from");
    printf("chat %s from %s is %d\n",chat->id,c->to->hexname,perm);
    if(perm < 0) return (chat_t)chan_fail(c,"blocked");
    if(perm == 0 && id) return (chat_t)chan_fail(c,"read-only");

    // legit new chat conn
    r = c->arg = chatr_new(chat);
    xht_set(chat->conn,c->to->hexname,c);

    // add to roster if given
    if(id) chat_add(chat,c->to->hexname,id);
    
    // re-fetch roster if hashes don't match
    if(util_cmp(packet_get_str(p,"roster"),chat->rhash) != 0) chat_cache(chat,chat->origin->hexname,NULL);

    // respond
    p = chan_packet(c);
    if(chat->join)
    {
      r->joined = 1;
      packet_set_str(p,"from",chat->join);
    }
    packet_set_str(p,"roster",chat->rhash);
    chan_send(c,p);

    return chat;
  }

  while((p = chan_pop(c)))
  {
    printf("chat packet %.*s\n", p->json_len, p->json);
    packet_free(p);
  }
  
  if(c->state == ENDING || c->state == ENDED)
  {
    // if it's this channel in the index, zap it
    if(xht_get(r->chat->conn,c->to->hexname) == c) xht_set(r->chat->conn,c->to->hexname,NULL);
    chatr_free(r);
    c->arg = NULL;
  }

  return NULL;
}
