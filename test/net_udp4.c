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
  fail_unless(net_udp4_socket(netA) > 0);

  net_udp4_t netB = net_udp4_new(meshB, NULL);
  fail_unless(netB);
  fail_unless(net_udp4_socket(netA) > 0);
  
  link_t linkAB = link_keys(meshA, meshB->keys);
  link_t linkBA = link_keys(meshB, meshA->keys);
  fail_unless(linkAB);
  fail_unless(linkBA);
  
  // kickstart direct
  net_udp4_direct(netA,link_handshake(linkAB),"127.0.0.1",net_udp4_port(netB));

  return 0; // WIP

  int i;
  for(i=256;i;i--)
  {
    net_udp4_process(netA);
    net_udp4_process(netB);
    if(link_up(linkAB) && link_up(linkBA)) break;
  }
  fail_unless(i);

  return 0;
}

