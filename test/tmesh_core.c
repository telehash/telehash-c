#include "tmesh.h"
#include "util_sys.h"
#include "util_unix.h"
#include "unit_test.h"

#include "./tmesh_device.c"
extern struct radio_struct test_device;

int main(int argc, char **argv)
{
  fail_unless(!e3x_init(NULL)); // random seed
  fail_unless(radio_device(&test_device));
  
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);

  lob_t idB = e3x_generate();
  hashname_t hnB = hashname_keys(lob_linked(idB));
  fail_unless(hnB);
  link_t link = link_get(meshA,hnB->hashname);
  fail_unless(link);
  
  tmesh_t netA = tmesh_new(meshA, NULL);
  fail_unless(netA);
  cmnty_t c = tmesh_join(netA,"qzjb5f4t","foo");
  fail_unless(c);
  fail_unless(c->medium->bin[0] == 134);
  fail_unless(c->pipe->path);
  LOG("netA %.*s",c->pipe->path->head_len,c->pipe->path->head);


  fail_unless(!tmesh_join(netA, "azdhpa5r", NULL));
  fail_unless((c = tmesh_join(netA, "azdhpa5n", "")));
  mote_t m = tmesh_link(netA, c, link);
  fail_unless(m);
  fail_unless(m->link == link);
  fail_unless(m->epochs);
  fail_unless(m == tmesh_link(netA, c, link));
//  fail_unless(m->ping);

  struct knock_struct k = {0,0,0,0,0,0,0};
  k.com = c;

  fail_unless(mote_knock(m, &k, 1));
  fail_unless(k.mote == m);
  LOG("%d %d %d",k.start,k.stop,k.chan);
  fail_unless(k.start);
  fail_unless(k.stop == (k.start + 10));
  fail_unless(k.chan < 100);
  uint8_t chan = k.chan;

  k.chan = 101; // set to bad value to make sure prep resets it
  fail_unless(tmesh_knock(netA, &k, 1, &test_device));
  fail_unless(m == k.mote);
  fail_unless(k.chan == chan);
  uint8_t frame[64];
  fail_unless(tmesh_knocking(netA, &k, frame));

  return 0;
}

