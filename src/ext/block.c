#include "ext.h"

// individual pipe local info
typedef struct ext_block_struct
{
  link_t link;
  e3x_channel_t chan;
  uint32_t min;
  lob_t cache;
  struct ext_block_struct *next;
} *ext_block_t;

// handle incoming packets for the built-in block channel
void block_chan_handler(link_t link, e3x_channel_t chan, void *arg)
{
  lob_t packet, last;
  ext_block_t block = arg;
  if(!link) return;

  // just append all packets, processed during block_receive()
  last = block->cache;
  while(last && last->next) last = last->next;
  while((packet = e3x_channel_receiving(chan)))
  {
    if(!last)
    {
      block->cache = last = packet;
      continue;
    }
    last->next = packet;
    last = packet;
  }
}

// new incoming block channel, set up handler
lob_t block_on_open(link_t link, lob_t open)
{
  ext_block_t block;
  if(!link) return open;
  if(lob_get_cmp(open,"type","block")) return open;

  if((block = xht_get(link->index, "block")))
  {
    LOG("note: new incoming block channel replacing existing one");
    // TODO delete old channel
  }else{
    LOG("incoming block channel open");

    // create new block
    if(!(block = malloc(sizeof (struct ext_block_struct)))) return LOG("OOM");
    memset(block,0,sizeof (struct ext_block_struct));
    block->link = link;
    // add to list of all blocks
    block->next = xht_get(link->mesh->index, "blocks");
    xht_set(link->mesh->index, "blocks", block);
  }

  // create new channel for this block handler
  block->chan = link_channel(link, open);
  link_handle(link,block->chan,block_chan_handler,block);

  return NULL;
}

// get the next incoming block, if any, packet->arg is the link it came from
lob_t ext_block_receive(mesh_t mesh)
{
  ext_block_t block;
  if(!mesh) return LOG("bad args");
  block = xht_get(mesh->index, "blocks");
  for(;block && block->cache; block = block->next)
  {
    // TODO get next block and remove/return it
  }

  return NULL;
}

// creates/reuses a single default block channel on the link
link_t ext_block_send(link_t link, lob_t block)
{
  e3x_channel_t chan;
  if(!link || !block) return LOG("bad args");
  
  if(!(chan = xht_get(link->index,"block")))
  {
    // TODO create outgoing channel
    xht_set(link->index,"block",chan);
  }
  
  // break block into packets and send

  return link;
}

mesh_t ext_block(mesh_t mesh)
{
  // set up built-in block channel handler
  mesh_on_open(mesh, "ext_block", block_on_open);
  return mesh;
}
