#include <stdio.h>
#include <string.h>

#include "packet.h"
#include "util.h"

int main(void)
{
  unsigned char out[4096];
  packet_t p,p2;
  p = packet_new();
  printf("empty packet %s\n",util_hex(packet_raw(p),packet_len(p),out));
  packet_json(p,(unsigned char*)"{\"foo\":\"bar\"}",strlen("{\"foo\":\"bar\"}"));
  packet_body(p,(unsigned char*)"BODY",4);
  printf("full packet %s\n",util_hex(packet_raw(p),packet_len(p),out));
  p2 = packet_copy(p);
  printf("copy packet %s\n",util_hex(packet_raw(p2),packet_len(p2),out));
  return 0;
}
