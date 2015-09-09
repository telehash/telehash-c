#ifndef ext_sock_h
#define ext_sock_h
/*
#include "ext.h"

#define SOCKC_NEW 0
#define SOCKC_OPEN 1
#define SOCKC_CLOSED 2

typedef struct sockc_struct 
{
  uint8_t state;
  uint32_t readable, writing;
  uint8_t *readbuf, *writebuf, *zwrite;
  lob_t opts;
  chan_t c;
  int fd; // convenience for app usage, initialized to -1
} *sockc_t;

// process channel for new incoming sock channel, any returned sockc must have a read or close function called
// state is SOCKC_OPEN or SOCKC_NEW (until sockc_accept())
// it's set to SOCKC_CLOSED after all the data is read on an ended channel or sockc_close() is called
sockc_t ext_sock(chan_t c);

// changes state from SOCKC_NEW to SOCKC_OPEN
void sockc_accept(sockc_t sc);

// create a sock channel to this hn, optional opts (ip, port), sets state=SOCKC_OPEN
sockc_t sockc_connect(switch_t s, char *hn, lob_t opts);

// tries to flush and end, cleans up, sets SOCKC_CLOSED
sockc_t sockc_close(sockc_t sock);

// must be called to free up sc resources (internally calls sockc_close to be sure)
sockc_t sockc_free(sockc_t sc);

// -1 on err, returns bytes read into buf up to len, sets SOCKC_CLOSED when channel ended
int sockc_read(sockc_t sock, uint8_t *buf, int len);

// -1 on err, returns len and will always buffer up to len 
int sockc_write(sockc_t sock, uint8_t *buf, int len);

// general flush outgoing buffer into a packet
void sockc_flush(chan_t c);

// advances readbuf by this len, for use when doing zero-copy reading directly from ->readbuf
// sets SOCKC_CLOSED when channel ended
void sockc_zread(sockc_t sc, int len);

// creates zero-copy buffer space of requested len at sc->zwrite or returns 0
int sockc_zwrite(sockc_t sc, int len);

// must use after writing anything to ->zwrite, adds len to outgoing buffer and resets sc->zwrite, returns len
int sockc_zwritten(sockc_t sc, int len);

// serial-style single character interface
int sockc_available(sockc_t sc);
uint8_t sockc_sread(sockc_t sc);
uint8_t sockc_swrite(sockc_t sc, uint8_t byte);
*/
#endif
