#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

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
  chat_t chat;
  int sock, len;
  char buf[64], nick[16];
  const int fd = fileno(stdin);
  const int fcflags = fcntl(fd,F_GETFL);
  fcntl(fd,F_SETFL,fcflags | O_NONBLOCK);

  crypt_init();
  s = switch_new();
  sprintf(nick,"%d",getpid());

  // make a dummy thtp response
  p = packet_new();
  packet_set_int(p,"status",200);
  packet_body(p,(unsigned char*)"bar\n",4);
  note = packet_new();
  packet_link(note,p);
  thtp_path(s,"/foo",note);
  
  if(util_loadjson(s) != 0 || (sock = util_server(0,100)) <= 0)
  {
    printf("failed to startup %s or %s\n", strerror(errno), crypt_err());
    return -1;
  }

  printf("loaded hashname %s\n",s->id->hexname);

  // new chat, must be after-init
  chat = chat_get(s,"tft");
  p = chat_message(chat);
  packet_set_str(p,"text",nick);
  chat_join(chat,p);
  printf("created chat %s %s %s\n",chat->id,packet_get_str(p,"id"),chat->rhash);

  link_hn(s, bucket_get(s->seeds, 0));
  util_sendall(s,sock);

  in = path_new("ipv4");
  while(util_readone(s, sock, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      printf("channel active %d %s %s\n",c->state,c->hexid,c->to->hexname);
      if(util_cmp(c->type,"connect") == 0) ext_connect(c);
      if(util_cmp(c->type,"thtp") == 0) ext_thtp(c);
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
    if((len = fread(buf,1,255,stdin)))
    {
      buf[len-1] = 0;
      if(strncmp(buf,"/nick ",6) == 0)
      {
        snprintf(nick,16,"%s",buf+6);
      }else if(strcmp(buf,"/quit") == 0){
        // TODO test freeing all
        return 0;
      }else if(strncmp(buf,"/get ",5) == 0){
        printf("get %s\n",buf+5);
      }else if(strncmp(buf,"/chat ",6) == 0){
        chat_free(chat);
        chat = chat_get(s,buf+6);
        p = chat_message(chat);
        packet_set_str(p,"text",nick);
        chat_join(chat,p);
        printf("joining chat %s %s %s\n",chat->id,packet_get_str(p,"id"),chat->rhash);
      }else{
        // default send as message
        p = chat_message(chat);
        packet_set_str(p,"text",buf);
        chat_send(chat,p);
      }
      printf("%s> ",nick);
    }
  }

  perror("exiting");
  return 0;
}
