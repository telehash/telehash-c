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

#define SBUF 1024

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

#define MODE_TOPORT 0
#define MODE_LISTEN 1
#define MODE_STDIN 2

void draino(int fd)
{
  if(fd < 0) return;
  fcntl(fd,F_SETFL,fcntl(fd,F_GETFL) | O_NONBLOCK);
}

int main(int argc, char *argv[])
{
  switch_t s;
  chan_t c;
  path_t in;
  sockc_t sc, sock = NULL;
  int udp, listener, port, mode, lin;
  char *hn;
  struct sockaddr_in sin;

  crypt_init();
  s = switch_new(0);

  if(util_loadjson(s) != 0 || (udp = util_server(0,100)) <= 0)
  {
    printf("failed to startup %s or %s\n", strerror(errno), crypt_err());
    return -1;
  }

  if(argc != 3)
  {
    printf("this hashname is %s\n",s->id->hexname);
    printf("test socket tunnel over telehash, source/destination can be hashname, port#, or '-' for stdin/stdout\n");
    printf("%s source dest\n",argv[0]);
    return 1;
  }

  if((hn = util_ishex(argv[1],64)) && (port = getport(argv[2])) >= 0)
  {
    // incoming sock channels create new socket to port
    mode = MODE_TOPORT;
  }else if((port = getport(argv[1])) >= 0 && (hn = util_ishex(argv[2],64))){
    // listen on given port, when socket (or if stdin) create sock channel to hashname
    if(port == 0)
    {
      mode = MODE_STDIN;
      sock = sockc_connect(s, hn, NULL);
      sock->fd = fileno(stdin);
      draino(sock->fd);
    }else{
      mode = MODE_LISTEN;
      memset(&sin, 0, sizeof(sin));
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = htonl(INADDR_ANY);
      sin.sin_port = htons(port);
      listener = socket(AF_INET, SOCK_STREAM, 0);
      if(bind(listener, (struct sockaddr *)&sin, sizeof(struct sockaddr)))
      {
        perror("bind failed");
        return 1;
      }
      draino(listener);
      listen(listener, 1);
    }
  }else{
    perror("invalid args, need a 64-char hex hashname in source or destination and port number or '-' in other");
    return 1;
  }


//  DEBUG_PRINTF("loaded hashname %s\n",s->id->hexname);

  in = path_new("ipv4");
  while(util_readone(s, udp, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      if(util_cmp(c->type,"connect") == 0) ext_connect(c);
      if(util_cmp(c->type,"sock") == 0 && (sc = ext_sock(c)))
      {
        // connect new sock to given port
        if(sc->state == SOCKC_NEW && mode == MODE_TOPORT && util_cmp(hn,c->to->hexname) == 0)
        {
          sockc_accept(sc);
          if(port == 0)
          {
            sc->fd = fileno(stdout);
          }else{
            sc->fd = socket(AF_INET, SOCK_STREAM, 0);
            memset(&sin, 0, sizeof(sin));
            sin.sin_family = AF_INET;
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            sin.sin_port = htons(port); 
            if(connect(sc->fd, (struct sockaddr *)&sin, sizeof(struct sockaddr))) sockc_close(sc);
          }
          draino(sc->fd);
        }

        // read/write any possible data
        while(sc->fd >= 0 && sc->readable)
        {
          sockc_zread(sc,write(sc->fd,sc->readbuf,sc->readable));
        }

        // only track the last active/open sockc in the main loop (we're just a testing utility)
        sock = (sc->state == SOCKC_OPEN)?sc:NULL;
        // clean up any closed ones
        if(sc->state == SOCKC_CLOSED)
        {
          if(port && sc->fd) close(sc->fd);
          sockc_free(sc);
        }
      }
    }

    util_sendall(s,udp);
    
    if(mode == MODE_LISTEN && (lin = accept(listener, NULL, NULL)) >= 0)
    {
      draino(lin);
      if(sock) sockc_close(sock);
      sock = sockc_connect(s, hn, NULL);
      sock->fd = lin;
    }
    
    // check any active socket and fd and write out any data
    if(sock && sock->fd >= 0) while(sockc_zwrite(sock,SBUF) && sockc_zwritten(sock,read(sock->fd,sock->zwrite,SBUF)));
  }

  perror("exiting");
  return 0;
}
