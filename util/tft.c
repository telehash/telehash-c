#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "switch.h"
#include "util.h"
#include "ext.h"
#include "util_unix.h"

int main(void)
{
  switch_t s;
  chan_t c;
  packet_t p, note;
  path_t in;
  thtp_t t;
  chat_t chat;
  int sock;

  crypt_init();
  s = switch_new();
  
  // make a thtp server and dummy response
  t = thtp_new(NULL);
  p = packet_new();
  packet_set_int(p,"status",200);
  packet_body(p,(unsigned char*)"bar\n",4);
  note = packet_new();
  packet_link(note,p);
  thtp_path(t,"/foo",note);
  
  if(util_loadjson(s) != 0 || (sock = util_server(0)) <= 0)
  {
    printf("failed to startup %s or %s\n", strerror(errno), crypt_err());
    return -1;
  }

  printf("loaded hashname %s\n",s->id->hexname);

  // new chat, must be after-init
  chat = chat_get(s,t,"foo");

  // create/send a ping packet  
  c = chan_new(s, bucket_get(s->seeds, 0), "link", 0);
  p = chan_packet(c);
  chan_send(c, p);
  util_sendall(s,sock);

  in = path_new("ipv4");
  while(util_readone(s, sock, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      printf("channel active %d %s %s\n",c->state,c->hexid,c->to->hexname);
      if(util_cmp(c->type,"connect") == 0) ext_connect(c);
      if(util_cmp(c->type,"thtp") == 0) ext_thtp(t,c);
      if(util_cmp(c->type,"chat") == 0) ext_chat(c);
      if(util_cmp(c->type,"link") == 0) ext_link(c);
      if(util_cmp(c->type,"seek") == 0) ext_seek(c);
      if(util_cmp(c->type,"path") == 0) ext_path(c);
      while((p = chan_pop(c)))
      {
        printf("unhandled channel packet %.*s\n", p->json_len, p->json);      
        packet_free(p);
      }
      if(c->state == ENDED) chan_free(c);
    }

    util_sendall(s,sock);
  }

  perror("exiting");
  return 0;
}
