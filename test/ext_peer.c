#include "ext.h"
#include "net_loopback.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);

  net_loopback_t pairAB = net_loopback_new(meshA,meshB);
  fail_unless(pairAB);

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

  // add a third
  mesh_t meshC = mesh_new(3);
  fail_unless(meshC);
  lob_t secretsC = mesh_generate(meshC);
  fail_unless(secretsC);

  // auto-accept A
  mesh_on_discover(meshC,"auto",mesh_add);

  net_loopback_t pairBC = net_loopback_new(meshB,meshC);
  fail_unless(pairBC);
  
  link_t linkBC = link_get(meshB, meshC->id->hashname);
  link_t linkCB = link_get(meshC, meshB->id->hashname);
  fail_unless(linkBC);
  fail_unless(linkCB);

  fail_unless(link_resync(linkBC));
  fail_unless(link_resync(linkCB));

  // B to handle peer requests and be a router
  mesh_on_open(meshB, "ext_peer", peer_on_open);

  // A to request to C, full link first
  link_t linkAC = mesh_add(meshA, mesh_json(meshC), NULL);
  fail_unless(linkAC);
  
  // dominooooooo
  peer_connect(linkAC, linkAB);

  return 0;
}

