#include "ext.h"

#define MUID "ext_link"

// handle incoming packets for the built-in link channel
void link_chan_handler(link_t link, channel3_t chan, void *arg)
{
  lob_t status, open, packet;
  if(!link) return;

  open = channel3_open(chan);
  while((packet = channel3_receiving(chan)))
  {
    lob_free(lob_unlink(open));
    if(lob_get(packet,"err"))
    {
      status = packet;
    }else{
      status = lob_parse(packet->body,packet->body_len);
      if(!status) status = lob_new(); // actual attached packet is optional, but we use it's existence for status
      lob_free(packet);
    }
    lob_link(open,status);

    // send out link update/change signal
    mesh_link(link->mesh, link);
  }
}

// new incoming link channel, set up handler
lob_t link_on_open(link_t link, lob_t open)
{
  channel3_t chan;
  if(!link) return open;
  if(lob_get_cmp(open,"type","link")) return open;
  
  if(xht_get(link->index, "link")) LOG("note: new incoming link channel replacing existing one");

  LOG("incoming link channel open");

  // create new channel, set it up, then receive this open
  chan = link_channel(link, open);
  link_handle(link,chan,link_chan_handler,NULL);
  xht_set(link->index,"link",chan);
  channel3_receive(chan,open);
  link_chan_handler(link,chan,NULL);
  return NULL;
}

// set/change the link status (err to mark down)
lob_t ext_link_status(link_t link, lob_t status)
{
  channel3_t chan;
  lob_t open, wrap;

  if(!link) return LOG("bad args");
  chan = (channel3_t)xht_get(link->index, "link");
  if(!chan)
  {
    if(!status) return LOG("link down");
    LOG("initiating new link channel");
    open = lob_new();
    lob_set(open,"type","link");
    lob_set_int(open,"seq",0); // reliable
    lob_body(open,lob_raw(status),lob_len(status));
    chan = link_channel(link,open);
    xht_set(link->index,"link",chan);
    link_handle(link,chan,link_chan_handler,NULL);
    return NULL;
  }
  
  open = channel3_open(chan);
  if(!open) return LOG("internal error");

  if(status)
  {
    
    if(lob_get(status,"err"))
    {
      lob_free(lob_linked(open)); // link down
      link_flush(link, chan, status);
      return NULL;
    }
    wrap = lob_new();
    lob_body(wrap,lob_raw(status),lob_len(status));
    link_flush(link, chan, wrap);
  }

  // return last received status (linked off the open)
  return lob_linked(open);
}

// auto-link
void link_on_link(link_t link)
{
  channel3_t chan;
  if(!link_ready(link)) return;

  chan = (channel3_t)xht_get(link->index, "link");
  if(lob_get(channel3_open(chan),"auto")) return; // already sent

  LOG("auto-linking");
  ext_link_status(link,lob_new());
  chan = (channel3_t)xht_get(link->index, "link"); // chan may have been created by status
  lob_set(channel3_open(chan),"auto","true");
}

mesh_t ext_link_auto(mesh_t mesh)
{
  ext_link(mesh);
  // watch link events to auto create/respond to link channel
  mesh_on_link(mesh, MUID, link_on_link);
  return mesh;
}

mesh_t ext_link(mesh_t mesh)
{
  // set up built-in link channel handler
  mesh_on_open(mesh, MUID, link_on_open);
  return mesh;
}
