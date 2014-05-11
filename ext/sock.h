#ifndef ext_sock_h
#define ext_sock_h

#include "ext.h"

#define SOCKC_NEW 0
#define SOCKC_OPEN 1
#define SOCKC_CLOSED 2

typedef struct sockc_struct 
{
  uint8_t state;
  uint32_t readable, writing;
  uint8_t *readbuf, *writebuf;
  packet_t opts;
  chan_t c;
} *sockc_t;

// process channel for new incoming sock channel, state is SOCKC_NEW until any read/write
sockc_t ext_sock(chan_t c);

// create a sock channel to this hn, optional opts (ip, port), sets open=1
sockc_t sockc_connect(switch_t s, hn_t hn, packet_t opts);

// tries to flush and end, cleans up, if not open it drops
void sockc_close(sockc_t sock);

// -1 on err, returns bytes read into buf up to len, sets open=1
int sockc_read(sockc_t sock, uint8_t *buf, int len);

// -1 on err, returns len and will always buffer up to len 
int sockc_write(sockc_t sock, uint8_t *buf, int len);

// general flush into a packet
void sockc_flush(chan_t c);

// advances readbuf by this len, for use when doing zero-copy reading directly from ->readbuf
void sockc_readup(sockc_t sc, uint32_t len);

#endif
