static struct radio_struct radioA, radioB;

medium_t device_get(radio_t self, tmesh_t tm, uint8_t medium[5])
{
  // each device only takes one mesh
  if(self == &radioA && tm != netA) return LOG("skip A");
  if(self == &radioB && tm != netB) return LOG("skip B");
  
  LOG("RADIO new medium for %s",(tm == netA) ? "netA" : "netB");

  medium_t m;
  if(!(m = malloc(sizeof(struct medium_struct)))) return LOG("OOM");
  memset(m,0,sizeof (struct medium_struct));
  memcpy(m->bin,medium,5);
  m->chans = 100;
  m->min = 10;
  m->max = 1000;
  m->z = 255;
  m->zshift = 4;
  return m;
}

medium_t device_free(radio_t self, tmesh_t tm, medium_t m)
{
  return NULL;
}

tmesh_t device_ready(radio_t self, tmesh_t tm, knock_t knock)
{
  LOG("RADIO READY %s %s",(tm == netA) ? "netA" : "netB",knock->tx?"TX":"RX");
  LOG("start %lu stop %lu on %d",knock->start,knock->stop,knock->chan);
  knock->done = knock->stop; // flag done here for testing
  return tm;
}

static struct radio_struct radioA = {
  device_get,
  device_free,
  device_ready,
  NULL,
  NULL,
  0
};

static struct radio_struct radioB = {
  device_get,
  device_free,
  device_ready,
  NULL,
  NULL,
  0
};

