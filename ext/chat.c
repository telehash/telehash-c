#include "ext.h"

// validate name part of an id
int chat_namelen(char *id)
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

chat_t chat_get(switch_t s, thtp_t t, char *id)
{
  chat_t ct;
  packet_t note;
  hn_t orig = NULL;
  int at;
  char buf[128];

  ct = xht_get(s->index,id);
  if(ct) return ct;

  // if there's an id, validate and optionally parse out originator
  if(id)
  {
    at = chat_namelen(id);
    if(at < 0) return NULL;
    if(at > 0)
    {
      id[at] = 0;
      orig = hn_gethex(s->index,id+(at+1));
      if(!orig) return NULL;
    }
  }

  ct = malloc(sizeof (struct chat_struct));
  memset(ct,0,sizeof (struct chat_struct));
  if(!id)
  {
    crypt_rand((unsigned char*)buf,4);
    util_hex((unsigned char*)buf,4,(unsigned char*)ct->name);
  }else{
    memcpy(ct->name,id,strlen(id)+1);
  }
  ct->orig = orig ? orig : s->id;
  sprintf(ct->id,"%s@%s",ct->name,ct->orig->hexname);
  ct->s = s;
  ct->t = t;

  // an admin channel for distribution and thtp requests
  ct->base = chan_new(s, s->id, "chat", 0);
  ct->base->arg = ct;
  note = chan_note(ct->base,NULL);
  sprintf(buf,"/chat/%s/",ct->name);
  thtp_glob(t,buf,note);

  xht_set(s->index,ct->id,ct);
  return ct;
}

chat_t chat_free(chat_t ct)
{
  if(!ct) return ct;
  xht_set(ct->s->index,ct->id,NULL);
  free(ct);
  return NULL;
}

chat_t ext_chat(chan_t c)
{
  packet_t p, note, req;
  chat_t ct = c->arg;
  char *path;
  
  // if this is the base channel, it just receives notes
  if(ct && ct->base == c)
  {
    while((note = chan_notes(c)))
    {
      req = packet_linked(note);
      printf("note req packet %.*s\n", req->json_len, req->json);
      path = packet_get_str(req,"path");
      p = packet_new();
      packet_set_int(p,"status",200);
      packet_body(p,(unsigned char*)path,strlen(path));
      packet_link(note,p);
      chan_reply(c,note);
    }
    return NULL;
  }

  while((p = chan_pop(c)))
  {
    printf("chat packet %.*s\n", p->json_len, p->json);      
    packet_free(p);
  }
  return NULL;
}