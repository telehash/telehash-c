#include <stdio.h>
#include <string.h>

#include "packet.h"
#include "util.h"
#include "crypt.h"
#include "hn.h"
#include "hnt.h"
#include "path.h"

int main(void)
{
  unsigned char out[4096], hn[64];
  packet_t p,p2;
  hn_t id;
  hnt_t seeds;
  path_t path;
  xht_t h;

  crypt_init();
  h = xht_new(4211);

  p = packet_new();
  printf("empty packet %s\n",util_hex(packet_raw(p),packet_len(p),out));
  packet_json(p,(unsigned char*)"{\"foo\":\"bar\"}",strlen("{\"foo\":\"bar\"}"));
  packet_body(p,(unsigned char*)"BODY",4);
  printf("full packet %s\n",util_hex(packet_raw(p),packet_len(p),out));
  p2 = packet_copy(p);
  packet_set_int(p,"num",42);
  packet_set_str(p,"foo","quote \"me\"");
  printf("copy packet json %.*s\n",p->json_len, p->json);

  id = hn_getfile(h, "id.json");
  if(!id)
  {
    printf("failed to load id.json: %s\n", crypt_err());
    return -1;
  }
  printf("loaded hashname %.*s\n",64,util_hex(id->hashname,32,hn));

  seeds = hn_getsfile(h, "seeds.json");
  if(!seeds)
  {
    printf("failed to load seeds.json: %s\n", crypt_err());
    return -1;
  }
  printf("loaded seed %.*s %s\n",64,util_hex((*(seeds->hns))->hashname,32,hn), path_json((*(seeds->hns))->paths[0]));

  path = path_new("ipv4");
  path_ip(path,"127.0.0.1");
  path_port(path, 42424);
  printf("path %s\n",path_json(path));

  return 0;
}
