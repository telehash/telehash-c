#include "tmesh.h"

uint32_t device_check(tmesh_t tm, uint8_t medium[6])
{
  return 1;
}

medium_t device_get(tmesh_t tm, uint8_t medium[6])
{
  medium_t m;
  if(!(m = malloc(sizeof(struct medium_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct medium_struct));
  memcpy(m->bin,medium,6);
  m->chans = 100;
  m->min = 10;
  m->max = 1000;
  return m;
}

medium_t device_free(tmesh_t tm, medium_t m)
{
  return NULL;
}

static struct radio_struct test_device = {
  0,
  device_check,
  device_get,
  device_free,
  NULL,
  {0}
};


