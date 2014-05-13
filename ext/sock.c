#include "ext.h"

sockc_t sockc_new(chan_t c)
{
  sockc_t sc;

  sc = malloc(sizeof(struct sockc_struct));
  memset(sc,0,sizeof (struct sockc_struct));
  sc->state = SOCKC_NEW;
  sc->c = c;
  c->arg = (void*)sc;
  return sc;
}

sockc_t sockc_free(sockc_t sc)
{
  if(!sc) return NULL;
  if(sc->readbuf) free(sc->readbuf);
  if(sc->writebuf) free(sc->writebuf);
  packet_free(sc->opts);
  if(sc->c) sc->c->arg = NULL;
  free(sc);
  return NULL;
}

// process channel for new incoming sock channel
sockc_t ext_sock(chan_t c)
{
  packet_t p;
  sockc_t sc = (sockc_t)c->arg;
  
  while((p = chan_pop(c)))
  {
    if(!sc)
    {
      c->arg = sc = sockc_new(c);
      sc->opts = packet_new();
      packet_set_json(sc->opts,p);
    }

    if(!c->state == CHAN_ENDING) sc->state = SOCKC_CLOSED;
    
    DEBUG_PRINTF("sock packet %d %d", sc->state, p->body_len);

    // copy in any payload
    if(p->body_len)
    {
      if(!(sc->readbuf = realloc(sc->readbuf,sc->readable+p->body_len)))
      {
        packet_free(p);
        sockc_free(sc);
        chan_fail(c,"oom");
        return NULL;
      }
      memcpy(sc->readbuf+sc->readable,p->body,p->body_len);
      sc->readable += p->body_len;
    }
    packet_free(p);
  }
  
  // optionally sends ack if needed
  chan_ack(c);
  
  // when ended, disconnect (to allow reading still)
  if(sc && c->s == CHAN_ENDED) sc->c = NULL;
  
  return sc;
}

// create a sock channel to this hn, optional opts (ip, port), sets open=1
sockc_t sockc_connect(switch_t s, hn_t hn, packet_t opts)
{
  packet_t p;
  sockc_t sc;

  // TODO start channel
  sc = sockc_new(chan_new(s, hn, "sock", 0));
  if(!sc) return NULL;
  sc->state = SOCKC_OPEN;
  p = chan_packet(sc->c);
  if(!p) return sockc_free(sc);
  packet_set_json(p,opts);
  packet_free(opts);
  chan_send(sc->c, p);
  return sc;
}

// tries to flush and end, cleans up, if not open it drops
void sockc_close(sockc_t sc)
{
  packet_t p;
  if(!sc || !sc->c || sc->state == SOCKC_CLOSED) return;
  sc->state = SOCKC_CLOSED;
  if(sc->writing) return sockc_flush(sc->c); // will end it
  p = chan_packet(sc->c);
  if(!p) return (void)chan_fail(sc->c,"overflow");
  chan_end(sc->c,p);
  chan_send(sc->c,p);
}

// -1 on err, returns bytes read into buf up to len, sets open=1
int sockc_read(sockc_t sc, uint8_t *buf, int len)
{
  if(len > (int)sc->readable) len = (int)sc->readable;
  memcpy(buf,sc->readbuf,len);
  sockc_readup(sc,(uint32_t)len);
  return len;
}

// send just one chunk
uint8_t sockc_chunk(sockc_t sc)
{
  packet_t p;
  uint32_t len;
  if(!sc || !(len = sc->writing)) return 0;
  p = chan_packet(sc->c);
  if(!p) return 0;
  if(len > packet_space(p)) len = packet_space(p);
  packet_append(p,sc->writebuf,len);
  sc->writing -= len;
  // optionally end with this packet if we're closed
  if(sc->state == SOCKC_CLOSED && !sc->writing) chan_end(sc->c,p);
  chan_send(sc->c,p);
  memmove(sc->writebuf,sc->writebuf+len,sc->writing);
  return 1;
}

// flush current packet buffer out
void sockc_flush(chan_t c)
{
  sockc_t sc = (sockc_t)c->arg;
  if(!sc) return;
  while(sockc_chunk(sc)); // send everything
  // come back again if there's more waiting
  sc->c->tick = (sc->writing) ? sockc_flush : NULL;
}

// -1 on err, returns len and will always buffer up to len 
int sockc_write(sockc_t sc, uint8_t *buf, int len)
{
  uint8_t *writebuf;
  if(sc->state == SOCKC_NEW) sc->state = SOCKC_OPEN;
  if(sc->state == SOCKC_CLOSED) return -1;
  
  writebuf = realloc(sc->writebuf,sc->writing+len);
  if(!writebuf) return -1;
  sc->writebuf = writebuf;
  memcpy(sc->writebuf+sc->writing,buf,len);
  sc->writing += len;
  sc->c->tick = sockc_flush; // create packet(s) during tick flush
  return len;
}

// advance the read buf, useful when accessing it directly for zero-copy
void sockc_readup(sockc_t sc, uint32_t len)
{
  if(!sc || !len) return;
  if(sc->state == SOCKC_NEW) sc->state = SOCKC_OPEN;
  if(len > sc->readable) len = sc->readable;
  sc->readable -= len;
  memmove(sc->readbuf,sc->readbuf+len,sc->readable);
  if(!(sc->readbuf = realloc(sc->readbuf,sc->readable))) sc->readable = 0;
}
