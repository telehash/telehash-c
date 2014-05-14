#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "switch.h"
#include "util.h"
#include "ext.h"
#include "util_unix.h"

int getport(char *str)
{
  char *bad;
  int port;
  if(!*str) return -1;
  if(str[0] == '-')
  {
    if(str[1] != 0) return -1;
    return 0;
  }
  port = (int)strtol(str,&bad,10);
  if(*bad) return -1;
  return port;
}

int main(int argc, char *argv[])
{
  switch_t s;
  chan_t c;
  packet_t p;
  path_t in;
  sockc_t sc;
  int sock, port;
  char *hn;
//  const int fd = fileno(stdin);
//  const int fcflags = fcntl(fd,F_GETFL);
//  fcntl(fd,F_SETFL,fcflags | O_NONBLOCK);

  if(argc != 3)
  {
    printf("test socket tunnel over telehash, source/destination can be hashname, port#, or '-' for stdin/stdout\n%s source dest\n",argv[0]);
    return 1;
  }

  if((hn = util_ishex(argv[1],64)) && (port = getport(argv[2])) >= 0)
  {
    // incoming sock channels create new socket to port
  }else if((port = getport(argv[1])) >= 0 && (hn = util_ishex(argv[2],64))){
    // listen on given port, when socket (or if stdin) create sock channel to hashname
  }else{
    perror("invalid args, need a 64-char hex hashname in source or destination and port number or '-' in other");
    return 1;
  }

  crypt_init();
  s = switch_new(0);

  if(util_loadjson(s) != 0 || (sock = util_server(0,100)) <= 0)
  {
    printf("failed to startup %s or %s\n", strerror(errno), crypt_err());
    return -1;
  }

//  DEBUG_PRINTF("loaded hashname %s\n",s->id->hexname);

  in = path_new("ipv4");
  while(util_readone(s, sock, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      if(c->handler) c->handler(c);
      else {
        if(util_cmp(c->type,"connect") == 0) ext_connect(c);
        if(util_cmp(c->type,"sock") == 0 && (sc = ext_sock(c)))
        {
//          if(sc->state == SOCKC_NEW) logg("SOCK NEW %s",c->to->hexname);
          while(sc->readable)
          {
//            logg("SOCK: %d %.*s",sc->readable,sc->readable,sc->readbuf);
            sockc_write(sc,sc->readbuf,sc->readable);
            sockc_readup(sc,sc->readable);
          }
//          if(sc->state == SOCKC_CLOSED) logg("SOCK CLOSED %s",c->to->hexname);
        }
      }

      // unhandled packets
      while((p = chan_pop(c))) packet_free(p);

      if(c->state == CHAN_ENDED) chan_free(c);
    }

    util_sendall(s,sock);
  }

  perror("exiting");
  return 0;
}
