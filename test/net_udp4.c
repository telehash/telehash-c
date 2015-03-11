#include "net_udp4.h"
#include "util_sys.h"
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
  
  net_udp4_t netA = net_udp4_new(meshA, NULL);
  fail_unless(netA);
  fail_unless(netA->port > 0);
  fail_unless(netA->path);
  fail_unless(lob_match(meshA->paths,"type","udp4"));
  LOG("netA %.*s",netA->path->head_len,netA->path->head);

  net_udp4_t netB = net_udp4_new(meshB, NULL);
  fail_unless(netB);
  fail_unless(netB->port > 0);
  LOG("netB %.*s",netB->path->head_len,netB->path->head);
  
  link_t linkAB = link_keys(meshA, meshB->keys);
  link_t linkBA = link_keys(meshB, meshA->keys);
  fail_unless(linkAB);
  fail_unless(linkBA);
  
  fail_unless(link_path(linkAB,netB->path));
  fail_unless(link_path(linkBA,netA->path));
  
  link_sync(linkAB);
  net_udp4_receive(netB);
  fail_unless(e3x_exchange_out(linkBA->x,0) >= e3x_exchange_out(linkAB->x,0));
  net_udp4_receive(netA);
  fail_unless(e3x_exchange_out(linkBA->x,0) == e3x_exchange_out(linkAB->x,0));

  return 0;
}

