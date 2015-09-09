#include "telehash.h"

sockc_t sockc_new(chan_t c)
{
  sockc_t sc;

  sc = malloc(sizeof(struct sockc_struct));
  memset(sc,0,sizeof (struct sockc_struct));
  sc->state = SOCKC_NEW;
  sc->fd = -1; // for app usage
  sc->c = c;
  c->arg = (void*)sc;
  return sc;
}

// process channel for new incoming sock channel
sockc_t ext_sock(chan_t c)
{
  lob_t p;
  sockc_t sc = (sockc_t)c->arg;

  DEBUG_PRINTF("sockc active %d",sc);
  while((p = chan_pop(c)))
  {
    if(!sc)
    {
      sc = sockc_new(c);
      sc->opts = lob_new();
      lob_set_json(sc->opts,p);
    }
    
    DEBUG_PRINTF("sock packet %d %d", sc->state, p->body_len);

    // copy in any payload
    if(p->body_len)
    {
      if(!(sc->readbuf = realloc(sc->readbuf,sc->readable+p->body_len)))
      {
        chan_fail(c,"oom");
      }else{
        memcpy(sc->readbuf+sc->readable,p->body,p->body_len);
        sc->readable += p->body_len;
      }
    }
    lob_free(p);
  }
  
  return sc;
}

void sockc_accept(sockc_t sc)
{
  if(!sc || sc->state != SOCKC_NEW) return;
  sc->state = SOCKC_OPEN;
  // send an empty packet (no options) to accept
  chan_send(sc->c,chan_packet(sc->c));
}

// create a sock channel to this hn, optional opts (ip, port)
sockc_t sockc_connect(switch_t s, char *hn, lob_t opts)
{
  lob_t p;
  sockc_t sc;

  sc = sockc_new(chan_start(s, hn, "sock"));
  if(!sc) return NULL;
  sc->state = SOCKC_OPEN;
  p = chan_packet(sc->c);
  lob_set_json(p,opts);
  lob_free(opts);
  chan_send(sc->c, p);
  return sc;
}

// util to just send a closing signal
void sockc_shutdown(sockc_t sc)
{
  if(!sc || sc->state == SOCKC_CLOSED) return;
  DEBUG_PRINTF("sockc shutdown %d",sc->c->id);
  sc->state = SOCKC_CLOSED;
    // try to flush any data and send the end w/ it
  sockc_flush(sc->c);
  // hard fail the channel if we couldn't flush
  if(sc->writing) chan_fail(sc->c,"overflow");
}

// tries to flush and end, cleans up/frees sc
sockc_t sockc_close(sockc_t sc)
{
  if(!sc) return NULL;
  sockc_shutdown(sc);
  // remove channel reference
  sc->c->arg = NULL;
  sc->c->tick = NULL;
  sc->c = NULL;
  if(sc->readbuf) free(sc->readbuf);
  if(sc->writebuf) free(sc->writebuf);
  lob_free(sc->opts);
  free(sc);
  return NULL;
}

// -1 on err, returns bytes read into buf up to len
int sockc_read(sockc_t sc, uint8_t *buf, int len)
{
  if(!sc || sc->state != SOCKC_OPEN) return -1;
  if(len > (int)sc->readable) len = (int)sc->readable;
  memcpy(buf,sc->readbuf,len);
  sockc_zread(sc,len);
  return len;
}

// send just one chunk
uint8_t sockc_chunk(sockc_t sc)
{
  lob_t p;
  uint32_t len;
  if(!sc) return 0;
  // if nothing to do, try sending an ack
  if(!sc->readable && !sc->writing && sc->state == SOCKC_OPEN)
  {
    chan_ack(sc->c);
    return 0;
  }
  // we've got something to say
  len = sc->writing;
  p = chan_packet(sc->c);
  if(!p) return 0;
  if(len > lob_space(p)) len = lob_space(p);
  lob_append(p,sc->writebuf,len);
  sc->writing -= len;
  // optionally end with the last packet if we're closed
  if(sc->state == SOCKC_CLOSED && !sc->writing) chan_end(sc->c,p);
  else chan_send(sc->c,p);
  memmove(sc->writebuf,sc->writebuf+len,sc->writing);
  return 1;
}

// flush current packet buffer out
void sockc_flush(chan_t c)
{
  sockc_t sc;
  if(!c) return;
  if(!(sc = (sockc_t)c->arg)) return;
  // send anything waiting
  while(sockc_chunk(sc));
  // come back again if there's more waiting
  sc->c->tick = (sc->state == SOCKC_OPEN && sc->writing) ? sockc_flush : NULL;
}

// -1 on err, returns len and will always buffer up to len 
int sockc_write(sockc_t sc, uint8_t *buf, int len)
{
  uint8_t *writebuf;
  if(!sc) return -1;
  if(len <= 0) return len;

  writebuf = realloc(sc->writebuf,sc->writing+len);
  if(!writebuf) return -1;
  sc->writebuf = writebuf;
  memcpy(sc->writebuf+sc->writing,buf,len);
  sc->writing += len;
  sc->c->tick = sockc_flush; // create packet(s) during tick flush
  
  return len;
}

int sockc_zwrite(sockc_t sc, int len)
{
  uint8_t *writebuf;
  if(!sc) return 0;
  
  // create requested writeable space at the end
  writebuf = realloc(sc->writebuf,sc->writing+len);
  if(!writebuf) return 0;
  sc->writebuf = writebuf;
  sc->zwrite = sc->writebuf + sc->writing;
  return len;
}

// advance the written after using zwrite
int sockc_zwritten(sockc_t sc, int len)
{
  if(!sc) return -1;
  if(len > 0)
  {
    sc->writing += len;
    sc->c->tick = sockc_flush; // create packet(s) during tick flush
  }
  sc->zwrite = NULL;
  return len;
}

// advance the read buf, useful when accessing it directly for zero-copy
void sockc_zread(sockc_t sc, int len)
{
  if(!sc || len == 0) return;
    // incoming error causes close/fail
  if(len < 0)
  {
    DEBUG_PRINTF("sockc read error %d",len);
    len = sc->readable;
    sockc_shutdown(sc);
  }
  
  // zero out given read buffer
  if(len > (int)sc->readable) len = (int)sc->readable;
  sc->readable -= (uint32_t)len;
  memmove(sc->readbuf,sc->readbuf+len,sc->readable);
  if(!(sc->readbuf = realloc(sc->readbuf,sc->readable))) sc->readable = 0;
  
  // more data to read yet
  if(sc->readable) return; 
  
  // if the channel is ending and the data all read, the sock is now closed
  if(sc->c->ended) sockc_shutdown(sc);

  // always try to flush when done reading, sends ack
  sockc_flush(sc->c);
}

int sockc_available(sockc_t sc)
{
  if(!sc || sc->state != SOCKC_OPEN) return -1;
  return (int)sc->readable;
}

uint8_t sockc_sread(sockc_t sc)
{
  // TODO
  return 0;
}

uint8_t sockc_swrite(sockc_t sc, uint8_t byte)
{
  // TODO
  return 0;
}
