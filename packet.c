#include "packet.h"
#include <stdlib.h>

packet_t packet_new()
{
  packet_t p = malloc(sizeof (struct packet_struct));
  return p;
}

packet_t packet_copy(packet_t p)
{
  return p;
}

void packet_free(packet_t p)
{
  free(p);
}

void packet_init(packet_t p, unsigned char *raw, unsigned short len)
{
  
}

unsigned char *packet_raw(packet_t p)
{
  return p->raw;
}

unsigned short packet_len(packet_t p)
{
  return 2+p->json_len+p->body_len;
}

void packet_json(packet_t p, unsigned char *json, unsigned short len)
{
  
}

void packet_body(packet_t p, unsigned char *body, unsigned short len)
{
  
}
