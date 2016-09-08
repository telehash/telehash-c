#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "telehash.h"

gossip_t gossip_new(void)
{
  gossip_s z = {0,};
  z.x = 1;
  gossip_t ret = malloc(sizeof(gossip_s));
  memcpy(ret,&z,sizeof(gossip_s));
  return ret;
}
