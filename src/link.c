#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include "telehash.h"
#include "telehash.h"

link_t link_new(mesh_t mesh, hashname_t id)
{
  link_t link;

  if(!mesh || !id) return LOG("invalid args");

  LOG("adding link %s",hashname_short(id));
  if(!(link = malloc(sizeof (struct link_struct)))) return LOG("OOM");
  memset(link,0,sizeof (struct link_struct));

  link->id = hashname_dup(id);
  link->csid = 0x01; // default state
  link->mesh = mesh;
  link->next = mesh->links;
  mesh->links = link;

  return link;
}

void link_free(link_t link)
{
  if(!link) return;

  LOG("dropping link %s",hashname_short(link->id));
  mesh_t mesh = link->mesh;
  if(mesh->links == link)
  {
    mesh->links = link->next;
  }else{
    link_t li;
    for(li = mesh->links;li;li = li->next) if(li->next == link)
    {
      li->next = link->next;
    }
  }

  // drop
  if(link->x)
  {
    e3x_exchange_free(link->x);
    link->x = NULL;
  }

  // notify pipe w/ NULL packet
  if(link->send_cb) link->send_cb(link, NULL, link->send_arg);

  // go through link->chans
  chan_t c, cnext;
  for(c = link->chans;c;c = cnext)
  {
    cnext = chan_next(c);
    chan_free(c);
  }

  hashname_free(link->id);
  lob_free(link->key);
  free(link);
}

link_t link_get(mesh_t mesh, hashname_t id)
{
  link_t link;

  if(!mesh || !id) return LOG("invalid args");
  for(link = mesh->links;link;link = link->next) if(hashname_cmp(id,link->id) == 0) return link;
  return link_new(mesh,id);
}

// simple accessors
hashname_t link_id(link_t link)
{
  if(!link) return NULL;
  return link->id;
}

lob_t link_key(link_t link)
{
  if(!link) return NULL;
  return link->key;
}

// get existing channel id if any
chan_t link_chan_get(link_t link, uint32_t id)
{
  chan_t c;
  if(!link || !id) return NULL;
  for(c = link->chans;c;c = chan_next(c)) {
    LOG("<%d><%d>",chan_id(c), id);
    if(chan_id(c) == id) return c;
  }
  return NULL;
}

// get link info json
lob_t link_json(link_t link)
{
  char hex[3];
  lob_t json;
  if(!link) return LOG("bad args");

  json = lob_new();
  lob_set(json,"hashname",hashname_char(link->id));
  lob_set(json,"csid",util_hex(&link->csid, 1, hex));
  lob_set_base32(json,"key",link->key->body,link->key->body_len);
//  paths = lob_array(mesh->paths);
//  lob_set_raw(json,"paths",0,(char*)paths->head,paths->head_len);
//  lob_free(paths);
  return json;
}

link_t link_get_keys(mesh_t mesh, lob_t keys)
{
  uint8_t csid;

  if(!mesh || !keys) return LOG("invalid args");
  csid = hashname_id(mesh->keys,keys);
  if(!csid) return LOG("no supported key");
  lob_t key = hashname_im(keys,csid);
  link_t ret = link_get_key(mesh, key, csid);
  lob_free(key);
  return ret;
}

link_t link_get_key(mesh_t mesh, lob_t key, uint8_t csid)
{
  link_t link;

  if(!mesh || !key) return LOG("invalid args");
  if(hashname_id(mesh->keys,key) > csid) return LOG("invalid csid");

  link = link_get(mesh, hashname_vkey(key, csid));
  if(!link) return LOG("invalid key");

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
  if(link->x)
  {
    link->csid = link->x->csid; // repair in case mesh_unlink was called, any better place?
    return link;
  }

  LOG("adding %x key to link %s",csid,hashname_short(link->id));

  // key must be bin
  if(key->body_len)
  {
    copy = lob_new();
    lob_body(copy,key->body,key->body_len);
  }else{
    util_hex(&csid,1,hex);
    copy = lob_get_base32(key,hex);
  }
  link->x = e3x_exchange_new(link->mesh->self, csid, copy);
  if(!link->x)
  {
    LOG("invalid %x key %s %s",csid,util_hex(copy->body,copy->body_len,NULL),lob_json(key));
    lob_free(copy);
    return NULL;
  }

  link->csid = csid;
  link->key = copy;

  e3x_exchange_out(link->x, util_sys_seconds());
  LOG("new exchange session to %s",hashname_short(link->id));

  return link;
}

// add a delivery pipe to this link
link_t link_pipe(link_t link, link_t (*send)(link_t link, lob_t packet, void *arg), void *arg)
{
  if(!link || !send) return NULL;

  if(send == link->send_cb && arg == link->send_arg) return link; // noop
  if(link->send_cb) LOG_INFO("replacing existing pipe on link");

  link->send_cb = send;
  link->send_arg = arg;

  // flush handshake
  return link_sync(link);
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

// process an incoming handshake
link_t link_receive_handshake(link_t link, lob_t inner)
{
  uint32_t in, out, at, err;
  uint8_t csid = 0;
  lob_t outer = lob_linked(inner);

  if(!link || !inner || !outer) return LOG("bad args");

  // inner/link must be validated by caller already, we just load if missing
  if(!link->key)
  {
    util_unhex(lob_get(inner, "csid"), 2, &csid);
    if(!link_load(link, csid, inner))
    {
      lob_free(inner);
      return LOG("load key failed for %s %u %s",hashname_short(link->id),csid,util_hex(inner->body,inner->body_len,NULL));
    }
  }

  if((err = e3x_exchange_verify(link->x,outer)))
  {
    lob_free(inner);
    return LOG("handshake verification fail: %d",err);
  }

  in = e3x_exchange_in(link->x,0);
  out = e3x_exchange_out(link->x,0);
  at = lob_get_uint(inner,"at");
  link_t ready = link_up(link);

  // if bad at, always send current handshake
  if(e3x_exchange_in(link->x, at) < out)
  {
    LOG("old handshake: %s (%d,%d,%d)",lob_json(inner),at,in,out);
    link_sync(link);
    lob_free(inner);
    return link;
  }

  // try to sync ephemeral key
  if(!e3x_exchange_sync(link->x,outer))
  {
    lob_free(inner);
    return LOG("sync failed");
  }

  // we may need to re-sync
  if(out != e3x_exchange_out(link->x,0)) link_sync(link);

  // notify of ready state change
  if(!ready && link_up(link))
  {
    LOG("link ready");
    mesh_link(link->mesh, link);
  }

  lob_free(inner);
  return link;
}

// forward declare
chan_t link_process_chan(chan_t c, uint32_t now);
// process a decrypted channel packet
link_t link_receive(link_t link, lob_t inner)
{
  chan_t c;

  if(!link || !inner) return LOG("bad args");

  LOG("<-- %d",lob_get_int(inner,"c"));
  // see if existing channel and send there
  if((c = link_chan_get(link, lob_get_int(inner,"c"))))
  {
    LOG("found chan");
    // consume inner
    chan_receive(c, inner);
    // process any changes
    link->chans = link_process_chan(link->chans, 0);
    return link;
  }

  // if it's an open, validate and fire event
  if(!lob_get(inner,"type"))
  {
    LOG("invalid channel open, no type %s",lob_json(inner));
    lob_free(inner);
    return NULL;
  }
  if(!e3x_exchange_cid(link->x, inner))
  {
    LOG("invalid channel open id %s",lob_json(inner));
    lob_free(inner);
    return NULL;
  }
  inner = mesh_open(link->mesh,link,inner);
  if(inner)
  {
    LOG("unhandled channel open %s",lob_json(inner));
    lob_free(inner);
    return NULL;
  }

  return link;
}

// deliver this packet
link_t link_send(link_t link, lob_t outer)
{
  if(!outer) return LOG_INFO("send packet missing");
  if(!link || !link->send_cb)
  {
    lob_free(outer);
    return LOG_WARN("no network");
  }

  if(!link->send_cb(link, outer, link->send_arg))
  {
    lob_free(outer);
    return LOG_WARN("delivery failed");
  }

  return link;
}

lob_t link_handshake(link_t link)
{
  if(!link) return NULL;
  if(!link->x) return LOG_DEBUG("no exchange");

  LOG_DEBUG("generating a new handshake in %lu out %lu",link->x->in,link->x->out);
  lob_t handshake = lob_new();
  lob_t tmp = hashname_im(link->mesh->keys, link->csid);
  lob_body(handshake, lob_raw(tmp), lob_len(tmp));
  lob_free(tmp);

  // encrypt it
  tmp = handshake;
  handshake = e3x_exchange_handshake(link->x, tmp);
  lob_free(tmp);

  return handshake;
}

// send current handshake
link_t link_sync(link_t link)
{
  if(!link) return LOG("bad args");
  if(!link->x) return LOG("no exchange");
  if(!link->send_cb) return LOG("no network");

  return link_send(link, link_handshake(link));
}

// trigger a new exchange sync
link_t link_resync(link_t link)
{
  if(!link) return LOG("bad args");
  if(!link->x) return LOG("no exchange");

  // force a higher at, triggers all to sync
  e3x_exchange_out(link->x,e3x_exchange_out(link->x,0)+1);
  return link_sync(link);
}

// create/track a new channel for this open
chan_t link_chan(link_t link, lob_t open)
{
  chan_t c;
  if(!link || !open) return LOG("bad args");

  // add an outgoing cid if none set
  if(!lob_get_int(open,"c")) lob_set_uint(open,"c",e3x_exchange_cid(link->x, NULL));
  c = chan_new(open);
  if(!c) return LOG("invalid open %s",lob_json(open));
  LOG("new outgoing channel %d open: %s",chan_id(c), lob_get(open,"type"));

  c->link = link;
  c->next = link->chans;
  link->chans = c;

  return c;
}

// encrypt and send this one packet on this pipe
link_t link_direct(link_t link, lob_t inner)
{
  if(!link || !inner) return LOG("bad args");
  if(!link->send_cb)
  {
    LOG_WARN("no network, dropping %s",lob_json(inner));
    return NULL;
  }

  // add an outgoing cid if none set
  if(!lob_get_int(inner,"c")) lob_set_uint(inner,"c",e3x_exchange_cid(link->x, NULL));

  lob_t outer = e3x_exchange_send(link->x, inner);
  lob_free(inner);

  return link_send(link, outer);
}

// force link down, end channels and generate all events
link_t link_down(link_t link)
{
  if(!link) return NULL;

  LOG("forcing link down for %s",hashname_short(link->id));

  // generate down event if up
  if(link_up(link))
  {
    e3x_exchange_down(link->x);
    mesh_link(link->mesh, link);
  }

  // end all channels
  chan_t c, cnext;
  for(c = link->chans;c;c = cnext)
  {
    cnext = chan_next(c);
    chan_err(c, "disconnected");
    chan_process(c, 0);
  }

  // remove pipe
  if(link->send_cb)
  {
    link->send_cb(link, NULL, link->send_arg); // notify jic
    link->send_cb = NULL;
    link->send_arg = NULL;
  }

  return NULL;
}

// recursive to handle deletes
chan_t link_process_chan(chan_t c, uint32_t now)
{
  if(!c) return NULL;
  chan_t next = link_process_chan(chan_next(c), now);
  if(!chan_process(c, now)) return next;
  c->next = next;
  return c;
}

// process any channel timeouts based on the current/given time
link_t link_process(link_t link, uint32_t now)
{
  if(!link || !now) return LOG("bad args");
  link->chans = link_process_chan(link->chans, now);
  if(link->csid) return link;

  // flagged to remove, do that now
  link_down(link);
  link_free(link);
  return NULL;
}
