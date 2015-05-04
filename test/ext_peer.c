#include "ext.h"
#include "net_loopback.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);
  fail_unless(ext_stream(meshA));

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  fail_unless(ext_stream(meshB));

  net_loopback_t pair = net_loopback_new(meshA,meshB);
  fail_unless(pair);
  
  link_t linkAB = link_get(meshA, meshB->id->hashname);
  link_t linkBA = link_get(meshB, meshA->id->hashname);
  fail_unless(linkAB);
  fail_unless(linkBA);

  fail_unless(link_resync(linkAB));
  fail_unless(link_resync(linkBA));

  // meta-test
  mesh_on_open(meshA, "ext_peer", peer_on_open);
  lob_t peerBA = lob_new();
  lob_set(peerBA,"type","peer");
  lob_set(peerBA,"peer",meshB->id->hashname);
  lob_set_uint(peerBA,"c",e3x_exchange_cid(linkBA->x, NULL));
  fail_unless(link_receive(linkAB, peerBA, NULL));

  // min test peer handling
  lob_t open = lob_new();
  lob_set(open,"type","peer");
  lob_set(open,"peer",meshB->id->hashname);
  fail_unless(peer_on_open(linkAB, open) == NULL);
  
  return 0;
}

