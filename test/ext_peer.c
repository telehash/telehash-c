#include "ext.h"
#include "net_loopback.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  lob_t secretsA = mesh_generate(meshA);
  fail_unless(secretsA);
  fail_unless(peer_enable(meshA));

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  lob_t secretsB = mesh_generate(meshB);
  fail_unless(secretsB);
  fail_unless(peer_enable(meshB));

  net_loopback_t pairAB = net_loopback_new(meshA,meshB);
  fail_unless(pairAB);

  link_t linkAB = link_get(meshA, meshB->id->hashname);
  link_t linkBA = link_get(meshB, meshA->id->hashname);
  fail_unless(linkAB);
  fail_unless(linkBA);

  fail_unless(link_resync(linkAB));
  fail_unless(link_resync(linkBA));

  // meta-test
  fail_unless(peer_route(meshA));
  lob_t peerAB = lob_new();
  lob_set(peerAB,"type","peer");
  lob_set(peerAB,"peer",meshB->id->hashname);
  lob_set_uint(peerAB,"c",e3x_exchange_cid(linkBA->x, NULL));
  fail_unless(link_receive(linkAB, peerAB, NULL));

  // add a third
  mesh_t meshC = mesh_new(3);
  fail_unless(meshC);
  lob_t secretsC = mesh_generate(meshC);
  fail_unless(secretsC);
  fail_unless(peer_enable(meshC));

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

  // B to be a router
  fail_unless(peer_route(meshB));

  // A to request to C, full link first
  link_t linkAC = mesh_add(meshA, mesh_json(meshC), NULL);
  fail_unless(linkAC);
  
  // dominooooooo
  peer_connect(linkAC, linkAB);

  fail_unless(link_up(linkAC));
  LOG("routed link connected");

  return 0;
}

