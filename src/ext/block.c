#include <string.h>
#include "telehash.h"

// individual pipe local info
typedef struct ext_block_struct
{
  chan_t chan;
  uint32_t min;
  lob_t cache;
  struct ext_block_struct *next;
} *ext_block_t;

// handle incoming packets for the built-in block channel
void block_chan_handler(chan_t chan, void *arg)
{
  lob_t packet, last;
  ext_block_t block = arg;
  if(!chan) return;

  // just append all packets, processed during block_receive()
  last = block->cache;
  while(last && last->next) last = last->next;
  while((packet = chan_receiving(chan)))
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
  ext_block_t block, blocks, b;
  if(!link) return open;
  if(lob_get_cmp(open,"type","block")) return open;

  // find existing for this link
  blocks = xht_get(link->mesh->index, "blocks");
  for(b=blocks,block=NULL;b;b=b->next) if(b->chan->link == link) block = b;

  if(block)
  {
    LOG("note: new incoming block channel replacing existing one");
    // TODO delete old channel
  }else{
    LOG("incoming block channel open");

    // create new block
    if(!(block = malloc(sizeof (struct ext_block_struct)))) return LOG("OOM");
    memset(block,0,sizeof (struct ext_block_struct));
    // add to list of all blocks
    block->next = blocks;
    xht_set(link->mesh->index, "blocks", block);
  }

  // create new channel for this block handler
  block->chan = link_chan(link, open);
  chan_handle(block->chan,block_chan_handler,block);

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
link_t ext_block_send(link_t link, lob_t data)
{
  ext_block_t block = NULL;
  if(!link || !data) return LOG("bad args");
  
  // TODO get existing block channel
  if(!block)
  {
    // TODO create outgoing channel
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
