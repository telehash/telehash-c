#include "net_udp4.h"
#include "unit_test.h"

static uint8_t state;

link_t handshake(mesh_t mesh, lob_t json, pipe_t pipe)
{
  LOG("HS-ACCEPT %s",lob_json(json));
  state = 1;
  lob_t hs = mesh_handshakes(mesh,json,"custom");
  lob_t key = mesh_handshakes(mesh,json,"link");
  if(!hs || !key) return LOG("incomplete %d %d",hs,key);
  state = 2;
  return mesh_add(mesh, key, pipe);
}

int main(int argc, char **argv)
{
  mesh_t meshA = mesh_new(3);
  fail_unless(meshA);
  mesh_generate(meshA);
  mesh_on_discover(meshA,"auto",mesh_add); // accept anyone
  net_udp4_t netA = net_udp4_new(meshA, NULL);
  fail_unless(netA);

  mesh_t meshB = mesh_new(3);
  fail_unless(meshB);
  mesh_generate(meshB);
  mesh_on_discover(meshB,"hstest",handshake); // accept only special handshake
  net_udp4_t netB = net_udp4_new(meshB, NULL);

  link_t linkAB = link_keys(meshA, meshB->keys);
  fail_unless(link_path(linkAB,netB->path));

//  link_t linkBA = link_get(meshB, meshA->id->hashname);
//  fail_unless(linkBA);

  state = 0;
  link_sync(linkAB);
  net_udp4_receive(netB);
  fail_unless(state == 1);

  lob_t hs = lob_new();
  lob_set(hs,"type","custom");
  link_handshake(linkAB,hs);
  fail_unless(link_resync(linkAB));
  net_udp4_receive(netB);
  net_udp4_receive(netB);
  fail_unless(state == 2);
  
  net_udp4_receive(netA);
  net_udp4_receive(netB);
  net_udp4_receive(netB);
  net_udp4_receive(netA);
  fail_unless(link_up(linkAB));

//  fail_unless(link_up(linkBA));


  return 0;
}

