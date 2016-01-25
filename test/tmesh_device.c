static struct radio_struct radioA, radioB;

medium_t device_get(radio_t self, medium_t m)
{
  tmesh_t tm = m->com->tm;
  // each device only takes one mesh
  if(self == &radioA && tm != netA) return LOG("skip A");
  if(self == &radioB && tm != netB) return LOG("skip B");
  
  LOG("RADIO new medium for %s",(tm == netA) ? "netA" : "netB");

  m->chans = 100;
  m->min = 10;
  m->max = 1000;
  m->z = 255;
  m->zshift = 4;
  return m;
}

medium_t device_free(radio_t self, medium_t m)
{
  return NULL;
}

medium_t device_ready(radio_t self, medium_t m, knock_t knock)
{
  tmesh_t tm = m->com->tm;
  LOG("RADIO READY %s %s",(tm == netA) ? "netA" : "netB",knock->tx?"TX":"RX");
  LOG("start %lu stop %lu on %d",knock->start,knock->stop,knock->chan);
  knock->done = knock->stop; // flag done here for testing
  return m;
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

