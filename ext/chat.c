#include "ext.h"

char *mmh32hex(const unsigned char* data, size_t len, char *hex);
uint32_t mmh32(const uint8_t * data, size_t len_);

// chatr is just per-chat-channel state holder
typedef struct chatr_struct 
{
  chat_t chat;
  packet_t in;
  int active;
} *chatr_t;

chatr_t chatr_new(chat_t chat)
{
  chatr_t r;
  r = malloc(sizeof (struct chatr_struct));
  memset(r,0,sizeof (struct chatr_struct));
  r->chat = chat;
  r->active = 0;
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
  mmh32hex((unsigned char*)buf,at,chat->rhash);
  return chat->rhash;
}

chat_t chat_get(switch_t s, char *id)
{
  chat_t chat;
  packet_t note;
  hn_t orig = NULL;
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
      orig = hn_gethex(s->index,id+(at+1));
      if(!orig) return NULL;
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
  chat->orig = orig ? orig : s->id;
  sprintf(chat->id,"%s@%s",chat->ep,chat->orig->hexname);
  chat->s = s;
  chat->roster = packet_new();
  chat->index = xht_new(101);
  chat->seq = 1000;
  crypt_rand((unsigned char*)&(chat->seed),4);

  // an admin channel for distribution and thtp requests
  chat->hub = chan_new(s, s->id, "chat", 0);
  chat->hub->arg = chatr_new(chat); // stub so we know it's the hub chat channel
  note = chan_note(chat->hub,NULL);
  packet_set_printf(note,"glob","/chat/%s/",mmh32hex((unsigned char*)chat->id,strlen(chat->id),buf));
  DEBUG_PRINTF("chat %s glob %s",chat->id,packet_get_str(note,"glob"));
  thtp_glob(s,0,note);
  xht_set(s->index,chat->id,chat);

  // any other hashname and we try to initialize
  if(chat->orig != s->id)
  {
    chat->state = LOADING;
    // TODO fetch roster and joins
  }else{
    chat->state = OFFLINE;
  }
  return chat;
}

chat_t chat_free(chat_t chat)
{
  if(!chat) return chat;
  xht_set(chat->s->index,chat->id,NULL);
  // TODO xht-walk chat->index and free packets
  // TODO unregister thtp
  xht_free(chat->index);
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
  util_hex((unsigned char*)chat->seed,4,(unsigned char*)id);
  for(step=0; step <= chat->seq; step++) mmh32hex(util_unhex((unsigned char*)id,8,(unsigned char*)buf),4,id);
  sprintf(id+8,",%d",chat->seq);
  chat->seq--;
  packet_set_str(p,"id",id);
  packet_set_str(p,"type","message");
  if(chat->after) packet_set_str(p,"after",chat->after);
  if(at > 1396184861) packet_set_int(p,"at",at); // only if platform_seconds() is epoch
  return p;
}

// fetch roster from origin
void chat_roster(chat_t chat)
{
  packet_t note;
  if(!chat || chat->orig == chat->s->id) return;
  note = chan_note(chat->hub,NULL);
  packet_set_str(note,"to",chat->orig->hexname);
  packet_set_printf(note,"path","/chat/%s/roster",chat->ep);
  thtp_req(chat->s,note);
}

// process roster to check connection/join state
void chat_sync(chat_t chat)
{
  char *part;
  chan_t c;
  chatr_t r;
  packet_t p;
  int i = 0;

  while((part = packet_get_istr(chat->roster,i++)))
  {
    if(!(c = xht_get(chat->index,part))) continue;
    r = c->arg;
    if(!r || r->active) continue;
    r->active = 1;
    p = chan_packet(c);
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
    p = xht_get(chat->index,chat->join);
    xht_set(chat->index,chat->join,NULL);
    packet_free(p);
  }
  chat->join = packet_get_str(join,"id");
  xht_set(chat->index,chat->join,join);
  packet_set_str(chat->roster,chat->s->id->hexname,chat->join);
  chat_rhash(chat);
  if(chat->orig == chat->s->id)
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
    if(!(c = xht_get(chat->index,part))) continue;
    r = c->arg;
    if(!r || !r->active) continue;
    chat_chunk(c,msg);
  }
  return NULL;
}

chat_t chat_default(chat_t chat, char *val)
{
  if(!chat) return NULL;
  packet_set_str(chat->roster,"*",val);
  chat_rhash(chat);
  return chat;
}

// this is the hub channel, it just receives notes
chat_t chat_hub(chat_t chat)
{
  packet_t p, note, req, resp;
  char *path, *thtp;
  
  while((note = chan_notes(chat->hub)))
  {
    thtp = packet_get_str(note,"thtp");
    if(util_cmp(thtp,"req") == 0)
    {
      req = packet_linked(note);
      printf("note req packet %.*s\n", req->json_len, req->json);
      path = packet_get_str(req,"path");
      p = packet_new();
      packet_set_int(p,"status",200);
      packet_body(p,(unsigned char*)path,strlen(path));
      packet_link(note,p);
      chan_reply(chat->hub,note);
    }
    if(util_cmp(thtp,"resp") == 0)
    {
      resp = packet_linked(note);
      printf("note resp packet %.*s\n", resp->json_len, resp->json);
      // TODO process roster or msg result
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

  // this is the hub channel, process it there
  if(r && r->chat->hub == c) return chat_hub(r->chat);

  if(!r)
  {
    if(!(p = chan_pop(c))) return (chat_t)chan_fail(c,"500");
    chat = chat_get(c->s,packet_get_str(p,"to"));
    if(!chat) return (chat_t)chan_fail(c,"500");
    printf("new chat %s from %s\n",chat->id,c->to->hexname);
    c->arg = chatr_new(chat);
    xht_set(chat->index,c->to->hexname,c);
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
    if(xht_get(r->chat->index,c->to->hexname) == c) xht_set(r->chat->index,c->to->hexname,NULL);
    chatr_free(r);
    c->arg = NULL;
  }

  return NULL;
}



// murmurhash stuffs

static inline uint32_t fmix(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

static inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

uint32_t mmh32(const uint8_t * data, size_t len_)
{
    const int len = (int) len_;
    const int nblocks = len / 4;

    uint32_t h1 = 0;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t * blocks = (const uint32_t*) (data + nblocks * 4);

    int i;
    for(i = -nblocks; i; i++)
    {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1*5+0xe6546b64;
    }

    //----------
    // tail

    const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

    uint32_t k1 = 0;

    switch(len & 3)
    {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
              k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
    }

    //----------
    // finalization

    h1 ^= len;

    h1 = fmix(h1);

    return h1;
}

char *mmh32hex(const unsigned char* data, size_t len, char *hex)
{
  uint32_t hash = mmh32((uint8_t*)data,len);
  sprintf(hex,"%08lx",(unsigned long)hash);
  return hex;
}