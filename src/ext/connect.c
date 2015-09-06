#include "telehash.h"

//unhandled channel packet {"from":{"3a":"459e76744a5a1e7f5f59e97f57f6524a8a84731917fbbdf746bdbfd2c4e2b4e7","2a":"81b441c63f11f6591ea89467a562077c73ed33bd6095349456eaca3893bb3ef9","1a":"d4e703ff112afeed53f5800511a33f8088385098"},"paths":[{"type":"ipv4","ip":"127.0.0.1","port":58919}],"type":"connect","c":5}

void ext_connect(chan_t c)
{
  lob_t p;
  hashname_t hn;
  while((p = chan_pop(c)))
  {
    hn = hashname_fromjson(c->s->index,p);
    lob_free(p);
    if(!hn) continue;
    DEBUG_PRINTF("connect HN %s\n",hn?hn->hexname:"null");
    switch_open(c->s, hn, NULL);
    // TODO relay
  }
}

