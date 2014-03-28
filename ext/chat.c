#include "ext.h"

struct chat_struct 
{
  char *id;
  hn_t orig;
  switch_t s;
  thtp_t t;
  chan_t base;
};

chat_t chat_get(switch_t s, thtp_t t, char *id)
{
  chat_t ct;
  packet_t note;
  // xht_get(s->index,id)
  ct = malloc(sizeof (struct chat_struct));
  memset(ct,0,sizeof (struct chat_struct));
//  if(!id) id = "";
  ct->id = strdup(id);
  ct->s = s;
  ct->t = t;
  ct->base = chan_new(s, s->id, "chat", 0);
  ct->base->arg = ct;
  note = chan_note(ct->base,NULL);
  thtp_glob(t,"/chat",note);
  return ct;
}

chat_t chat_free(chat_t ct)
{
  if(!ct) return ct;
  //xht_set(cs->s->index,ct->id)
  free(ct->id);
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