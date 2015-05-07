#include "link.h"
#include <string.h>
#include <stdlib.h>
#include "util.h"
#include "mesh.h"

// internal structures to manage our link-local state about pipes and channels

// list of active pipes and state per link
typedef struct seen_struct
{
  pipe_t pipe;
  uint32_t at;
  struct seen_struct *next;
} *seen_t;

// these sit in the xht index to wrap the e3x_channel and recipient handler
typedef struct chan_struct
{
  e3x_channel_t c3;
  void *arg;
  void (*handle)(link_t link, e3x_channel_t c3, void *arg);
} *chan_t;

link_t link_new(mesh_t mesh, hashname_t id)
{
  link_t link;

  if(!mesh || !id) return LOG("invalid args");

  LOG("adding link %s",id->hashname);
  if(!(link = malloc(sizeof (struct link_struct))))
  {
    hashname_free(id);
    return NULL;
  }
  memset(link,0,sizeof (struct link_struct));
  
  link->id = id;
  link->mesh = mesh;
  xht_set(mesh->index,id->hashname,link);

  // to size larger, app can xht_free(); link->channels = xht_new(BIGGER) at start itself
  link->channels = xht_new(5); // index of all channels
  link->index = xht_new(5); // index for active channels and extensions

  return link;
}

void link_free(link_t link)
{
  if(!link) return;
  LOG("dropping link %s",link->id->hashname);
  xht_set(link->mesh->index,link->id->hashname,NULL);

  // TODO go through ->pipes

  // TODO go through link->channels
  xht_free(link->channels);
  xht_free(link->index);

  hashname_free(link->id);
  if(link->x)
  {
    xht_set(link->mesh->index,link->token,NULL);
    e3x_exchange_free(link->x);
  }
  free(link);
}

link_t link_get(mesh_t mesh, char *hashname)
{
  link_t link;
  hashname_t id;

  if(!mesh || !hashname) return LOG("invalid args");
  link = xht_get(mesh->index,hashname);
  if(!link)
  {
    id = hashname_str(hashname);
    if(!id) return LOG("invalid hashname %s",hashname);
    link = link_new(mesh,id);
  }

  return link;
}

link_t link_keys(mesh_t mesh, lob_t keys)
{
  uint8_t csid;

  if(!mesh || !keys) return LOG("invalid args");
  csid = hashname_id(mesh->keys,keys);
  if(!csid) return LOG("no supported key");
  return link_key(mesh, hashname_im(keys,csid), csid);
}

link_t link_key(mesh_t mesh, lob_t key, uint8_t csid)
{
  hashname_t hn;
  link_t link;

  if(!mesh || !key) return LOG("invalid args");
  if(hashname_id(mesh->keys,key) > csid) return LOG("invalid csid");

  hn = hashname_key(key, csid);
  if(!hn) return LOG("invalid key");

  link = link_get(mesh, hn->hashname);
  if(link)
  {
    hashname_free(hn);
  }else{
    link = link_new(mesh,hn);
  }

  // load key if it's not yet
  if(!link->key) return link_load(link, csid, key);

  return link;
}

// load in the key to existing link
link_t link_load(link_t link, uint8_t csid, lob_t key)
{
  char hex[3];
  lob_t copy;

  if(!link || !csid || !key) return LOG("bad args");
  if(link->x) return link;

  LOG("adding %x key to link %s",csid,link->id->hashname);
  
  // key must be bin
  if(key->body_len)
  {
    copy = lob_copy(key);
  }else{
    util_hex(&csid,1,hex);
    copy = lob_get_base32(key,hex);
  }
  link->x = e3x_exchange_new(link->mesh->self, csid, copy);
  if(!link->x)
  {
    lob_free(copy);
    return LOG("invalid %x key %d %s",csid,key->body_len,lob_json(key));
  }

  link->csid = csid;
  link->key = copy;
  
  // route packets to this token
  util_hex(e3x_exchange_token(link->x),16,link->token);
  xht_set(link->mesh->index,link->token,link);
  e3x_exchange_out(link->x, util_sys_seconds());
  LOG("new session token %s to %s",link->token,link->id->hashname);

  return link;
}

// try to turn a path into a pipe
pipe_t link_path(link_t link, lob_t path)
{
  pipe_t pipe;

  if(!link || !path) return LOG("bad args");

  if(!(pipe = mesh_path(link->mesh, link, path))) return NULL;
  link_pipe(link, pipe);
  return pipe;
}

// add a pipe to this link if not yet
link_t link_pipe(link_t link, pipe_t pipe)
{
  seen_t seen;

  if(!link || !pipe) return LOG("bad args");

  // see if we've seen it already
  for(seen = link->pipes; seen; seen = seen->next)
  {
    if(seen->pipe == pipe) return link;
  }

  // add this pipe to this link
  LOG("adding pipe %s",pipe->id);
  if(!(seen = malloc(sizeof (struct seen_struct)))) return NULL;
  memset(seen,0,sizeof (struct seen_struct));
  seen->pipe = pipe;
  seen->next = link->pipes;
  link->pipes = seen;
  
  // make sure it gets sync'd
  lob_free(link_sync(link));

  return link;
}

// iterate through existing pipes for a link
pipe_t link_pipes(link_t link, pipe_t after)
{
  seen_t seen;
  if(!link) return LOG("bad args");

  // see if we've seen it already
  for(seen = link->pipes; seen; seen = seen->next)
  {
    if(!after) return seen->pipe;
    if(after == seen->pipe) after = NULL;
  }
  
  return NULL;
}

// is the link ready/available
link_t link_up(link_t link)
{
  if(!link) return NULL;
  if(!link->x) return NULL;
  if(!e3x_exchange_out(link->x,0)) return NULL;
  if(!e3x_exchange_in(link->x,0)) return NULL;
  return link;
}

// add a custom outgoing handshake packet
link_t link_handshake(link_t link, lob_t handshake)
{
  if(!link) return NULL;
  link->handshakes = lob_link(handshake, link->handshakes);
  return link;
}

// process an incoming handshake
link_t link_receive_handshake(link_t link, lob_t inner, pipe_t pipe)
{
  link_t ready;
  uint32_t out, err;
  seen_t seen;
  uint8_t csid = 0;
  char *hexid;
  lob_t attached, outer = lob_linked(inner);

  if(!link || !inner || !outer) return LOG("bad args");
  hexid = lob_get(inner, "csid");
  if(!lob_get(link->mesh->keys, hexid)) return LOG("unsupported csid %s",hexid);
  util_unhex(hexid, 2, &csid);
  attached = lob_parse(inner->body, inner->body_len);
  if(!link->key && link_key(link->mesh, attached, csid) != link) return LOG("invalid/mismatch link handshake");
  if((err = e3x_exchange_verify(link->x,outer))) return LOG("handshake verification fail: %d",err);

  out = e3x_exchange_out(link->x,0);
  ready = link_up(link);

  // if bad at, always send current handshake
  if(e3x_exchange_in(link->x, lob_get_uint(inner,"at")) < out)
  {
    LOG("old/bad at: %s (%d,%d,%d)",lob_json(inner),lob_get_int(inner,"at"),e3x_exchange_in(link->x,0),e3x_exchange_out(link->x,0));
    // just reset pipe seen and call link_sync to resend handshake
    for(seen = link->pipes;pipe && seen;seen = seen->next) if(seen->pipe == pipe) seen->at = 0;
    lob_free(link_sync(link));
    return NULL;
  }

  // trust/add this pipe
  if(pipe) link_pipe(link,pipe);

  // try to sync ephemeral key
  if(!e3x_exchange_sync(link->x,outer)) return LOG("sync failed");
  
  // we may need to re-sync
  if(out != e3x_exchange_out(link->x,0)) lob_free(link_sync(link));
  
  // notify of ready state change
  if(!ready && link_up(link))
  {
    LOG("link ready");
    mesh_link(link->mesh, link);
  }
  
  return link;
}

// process a decrypted channel packet
link_t link_receive(link_t link, lob_t inner, pipe_t pipe)
{
  chan_t chan;

  if(!link || !inner) return LOG("bad args");

  // see if existing channel and send there
  if((chan = xht_get(link->index, lob_get(inner,"c"))))
  {
    LOG("\t<-- %s",lob_json(inner));
    if(e3x_channel_receive(chan->c3, inner)) return LOG("channel receive error, dropping %s",lob_json(inner));
    if(pipe) link_pipe(link,pipe); // we trust the pipe at this point
    if(chan->handle) chan->handle(link, chan->c3, chan->arg);
    // check if there's any packets to be sent back
    return link_flush(link, chan->c3, NULL);
  }

  // if it's an open, validate and fire event
  if(!lob_get(inner,"type")) return LOG("invalid channel open, no type %s",lob_json(inner));
  if(!e3x_exchange_cid(link->x, inner)) return LOG("invalid channel open id %s",lob_json(inner));
  if(pipe) link_pipe(link,pipe); // we trust the pipe at this point
  inner = mesh_open(link->mesh,link,inner);
  if(inner)
  {
    LOG("unhandled channel open %s",lob_json(inner));
    lob_free(inner);
    return NULL;
  }
  
  return link;
}

// send this packet to the best pipe
link_t link_send(link_t link, lob_t outer)
{
  pipe_t pipe;
  
  if(!link) return LOG("bad args");
  if(!link->pipes || !(pipe = link->pipes->pipe)) return LOG("no network");

  pipe->send(pipe, outer, link);
  return link;
}

lob_t link_handshakes(link_t link)
{
  uint32_t i;
  uint8_t csid;
  char *key;
  lob_t tmp, hs = NULL, handshakes = NULL;
  if(!link) return NULL;
  
  // no keys means we have to generate a handshake for each key
  if(!link->x)
  {
    for(i=0;(key = lob_get_index(link->mesh->keys,i));i+=2)
    {
      util_unhex(key,2,&csid);
      hs = lob_new();
      tmp = hashname_im(link->mesh->keys, csid);
      lob_body(hs, lob_raw(tmp), lob_len(tmp));
      lob_free(tmp);
      handshakes = lob_link(hs, handshakes);
    }
  }else{ // generate one just for this csid
    handshakes = lob_new();
    tmp = hashname_im(link->mesh->keys, link->csid);
    lob_body(handshakes, lob_raw(tmp), lob_len(tmp));
    lob_free(tmp);
  }

  // add any custom per-link
  for(hs = link->handshakes; hs; hs = lob_linked(hs)) handshakes = lob_link(lob_copy(hs), handshakes);

  // add any mesh-wide handshakes
  for(hs = link->mesh->handshakes; hs; hs = lob_linked(hs)) handshakes = lob_link(lob_copy(hs), handshakes);
  
  // encrypt them if we can
  if(link->x)
  {
    tmp = handshakes;
    handshakes = NULL;
    for(hs = tmp; hs; hs = lob_linked(hs)) handshakes = lob_link(e3x_exchange_handshake(link->x, hs), handshakes);
    lob_free(tmp);
  }

  return handshakes;
}

// make sure all pipes have the current handshake
lob_t link_sync(link_t link)
{
  uint32_t at;
  seen_t seen;
  lob_t handshakes = NULL, hs = NULL;
  if(!link) return LOG("bad args");
  if(!link->x) return LOG("no exchange");

  at = e3x_exchange_out(link->x,0);
  LOG("link sync at %d",at);
  for(seen = link->pipes;seen;seen = seen->next)
  {
    if(!seen->pipe || !seen->pipe->send || seen->at == at) continue;

    // only create if we have to
    if(!handshakes) handshakes = link_handshakes(link);

    seen->at = at;
    for(hs = handshakes; hs; hs = lob_linked(hs)) seen->pipe->send(seen->pipe, lob_copy(hs), link);
  }

  // caller can re-use and must free
  return handshakes;
}

// trigger a new exchange sync
lob_t link_resync(link_t link)
{
  if(!link) return LOG("bad args");
  if(!link->x) return LOG("no exchange");

  // force a higher at, triggers all to sync
  e3x_exchange_out(link->x,e3x_exchange_out(link->x,0)+1);
  return link_sync(link);
}

// create/track a new channel for this open
e3x_channel_t link_channel(link_t link, lob_t open)
{
  chan_t chan;
  e3x_channel_t c3;
  if(!link || !open) return LOG("bad args");

  // add an outgoing cid if none set
  if(!lob_get_int(open,"c")) lob_set_uint(open,"c",e3x_exchange_cid(link->x, NULL));
  c3 = e3x_channel_new(open);
  if(!c3) return LOG("invalid open %s",lob_json(open));
  LOG("new outgoing channel open: %s",lob_get(open,"type"));

  // add this channel to the link's channel index
  if(!(chan = malloc(sizeof (struct chan_struct))))
  {
    e3x_channel_free(c3);
    return LOG("OOM");
  }
  memset(chan,0,sizeof (struct chan_struct));
  chan->c3 = c3;
  xht_set(link->channels, e3x_channel_uid(c3), chan);
  xht_set(link->index, e3x_channel_c(c3), chan);

  return c3;
}

// set up internal handler for all incoming packets on this channel
link_t link_handle(link_t link, e3x_channel_t c3, void (*handle)(link_t link, e3x_channel_t c3, void *arg), void *arg)
{
  chan_t chan;
  if(!link || !c3) return LOG("bad args");
  chan = xht_get(link->channels, e3x_channel_uid(c3));
  if(!chan) return LOG("unknown channel %s",e3x_channel_uid(c3));

  chan->handle = handle;
  chan->arg = arg;

  return link;
}

// process any outgoing packets for this channel, optionally send given packet too
link_t link_flush(link_t link, e3x_channel_t c3, lob_t inner)
{
  if(!link || !c3) return LOG("bad args");
  
  if(inner) e3x_channel_send(c3, inner);

  while((inner = e3x_channel_sending(c3)))
  {
    LOG("\t--> %s",lob_json(inner));
    link_send(link, e3x_exchange_send(link->x, inner));
    lob_free(inner);
  }
  
  // TODO if channel is now ended, remove from link->index

  return link;
}

// encrypt and send this one packet on this pipe
link_t link_direct(link_t link, lob_t inner, pipe_t pipe)
{
  if(!link || !inner) return LOG("bad args");
  if(!pipe && (!link->pipes || !(pipe = link->pipes->pipe))) return LOG("no network");

  // add an outgoing cid if none set
  if(!lob_get_int(inner,"c")) lob_set_uint(inner,"c",e3x_exchange_cid(link->x, NULL));

  pipe->send(pipe, e3x_exchange_send(link->x, inner), link);
  
  return link;
}
