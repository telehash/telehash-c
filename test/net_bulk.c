#include "ext.h"
#include "net_loopback.h"
#include "unit_test.h"

int bulked = 0;
void bulk_handler(chan_t chan, void *arg)
{
  lob_t packet;

  while((packet = chan_receiving(chan)))
  {
    chan_send(chan,chan_packet(chan));
    lob_free(packet);
    bulked++;
  }
  
}

chan_t chan = NULL;
lob_t bulk_on_open(link_t link, lob_t open)
{
  if(lob_get_cmp(open,"type","bulk")) return open;
  
  LOG("incoming bulk start");

  // create new channel, set it up, then receive this open
  chan = link_chan(link, open);
  chan_handle(chan,bulk_handler,NULL);
  lob_t reply = lob_new();
  lob_set(reply,"c",lob_get(open,"c"));
  chan_receive(chan,open); // consumes the open
  printf("Done setting up channel: %d\n", chan_size(chan));

  link_direct(link,reply);

  return NULL;
}


int main(int argc, char **argv)
{
  lob_t opt = lob_new();
  lob_set(opt,"force","1a");
  e3x_init(opt);
  lob_free(opt);

  mesh_t meshA = mesh_new();
  fail_unless(meshA);
  lob_t secretsA = e3x_generate();
  fail_unless(secretsA);
  fail_unless(!mesh_load(meshA, secretsA, lob_linked(secretsA)));
  lob_free(secretsA);

  mesh_t meshB = mesh_new();
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  lob_free(secretsB);

  net_loopback_t pair = net_loopback_new(meshA,meshB);
  fail_unless(pair);
  
  link_t linkAB = link_get(meshA, meshB->id);
  link_t linkBA = link_get(meshB, meshA->id);
  fail_unless(linkAB);
  fail_unless(linkBA);

  fail_unless(link_resync(linkAB));

  mesh_on_open(meshA, "bulk", bulk_on_open);
  lob_t bulkBA = lob_new();
  lob_set(bulkBA,"type","bulk");
  uint32_t cid = e3x_exchange_cid(linkBA->x, NULL);
  lob_set_uint(bulkBA,"c",cid);
  fail_unless(link_receive(linkAB, bulkBA));
  fail_unless(chan);

  int i;
  for(i=0;i<100;i++)
  {
    lob_t bulk = lob_new();
    lob_set_uint(bulk,"c",cid);
    fail_unless(link_receive(linkAB, bulk));
  }
  
  LOG("bulked %d",bulked);
  fail_unless(bulked == i+1);
  
  mesh_free(meshA);
  mesh_free(meshB);
  net_loopback_free(pair);

  return 0;
}

