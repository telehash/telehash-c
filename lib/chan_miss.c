#include <string.h>
#include <stdlib.h>
#include "switch.h"

typedef struct miss_struct
{
  packet_t out;
} *miss_t;

void chan_miss_init(chan_t c)
{
  miss_t m = malloc(sizeof (struct miss_struct));
  memset(m,0,sizeof (struct miss_struct));
  c->miss = (void*)m;
}

void chan_miss_free(chan_t c)
{
  // TODO free every out packet
  free(c->miss);
}

// buffers packets to be able to re-send
void chan_miss_send(chan_t c, packet_t p)
{
  
}

// looks at incoming miss/ack and resends or frees
void chan_miss_check(chan_t c, packet_t p)
{
  
}
