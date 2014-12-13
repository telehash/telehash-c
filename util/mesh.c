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

char nick[16];
void logg(char * format, ...)
{
    char buffer[1024];
    va_list args;
    va_start (args, format);
    vsnprintf (buffer, 1024, format, args);
    if(strlen(buffer))
    {
      printf("\n%s\n%s> ", buffer, nick);      
    }else{
      printf("%s> ",nick);
    }
    va_end (args);
}

int main(int argc, char *argv[])
{
  switch_t s;
  chan_t c, admin;
  lob_t p, note;
  path_t in;
  chat_t chat;
  sockc_t sc;
  int sock, len;
  char buf[256];
  const int fd = fileno(stdin);
  const int fcflags = fcntl(fd,F_GETFL);
  fcntl(fd,F_SETFL,fcflags | O_NONBLOCK);

  if(argc > 1) util_sys_debugging(1);

  crypt_init();
  s = switch_new(0);
  seek_auto(s);
  sprintf(nick,"%d",getpid());

  // make a dummy thtp response
  p = lob_new();
  lob_set_int(p,"status",200);
  lob_body(p,(unsigned char*)"bar\n",4);
  note = lob_new();
  lob_link(note,p);
  thtp_path(s,"/foo",note);
  
  if(util_loadjson(s) != 0 || (sock = util_server(0,100)) <= 0)
  {
    printf("failed to startup %s or %s\n", strerror(errno), crypt_err());
    return -1;
  }

  DEBUG_PRINTF("loaded hashname %s\n",s->id->hexname);

  // new chat, must be after-init
  chat = chat_get(s,"tft");
  chat_add(chat,"*","invite");
  p = chat_message(chat);
  lob_set_str(p,"text",nick);
  chat_join(chat,p);
  printf("created chat %s %s %s\n",chat->id,lob_get_str(p,"id"),chat->rhash);
  printf("%s> ",nick);

  // create an admin channel for notes
  admin = chan_new(s, s->id, ".admin", 0);

  // link the first seed only
  link_hn(s, bucket_get(s->seeds, 0), chan_note(admin,NULL));
  util_sendall(s,sock);

  in = path_new("ipv4");
  while(util_readone(s, sock, in) == 0)
  {
    switch_loop(s);

    while((c = switch_pop(s)))
    {
      // our internal testing stuff
      if(c == admin)
      {
        while((p = chan_notes(c)))
        {
          printf("admin note %.*s\n",p->json_len,p->json);
          lob_free(p);
        }
        continue;
      }

      DEBUG_PRINTF("channel active %d %s %s\n",c->ended,c->hexid,c->to->hexname);
      if(c->handler) c->handler(c);
      else {
        if(util_cmp(c->type,"connect") == 0) ext_connect(c);
        if(util_cmp(c->type,"thtp") == 0) ext_thtp(c);
        if(util_cmp(c->type,"link") == 0) ext_link(c);
        if(util_cmp(c->type,"seek") == 0) ext_link(c);
        if(util_cmp(c->type,"path") == 0) ext_path(c);
        if(util_cmp(c->type,"peer") == 0) ext_peer(c);
        if(util_cmp(c->type,"sock") == 0 && (sc = ext_sock(c)))
        {
          DEBUG_PRINTF("XXXX %d",sc);
          if(sc->state == SOCKC_NEW) logg("SOCK NEW %s",c->to->hexname);
          sockc_accept(sc);
          while(sc->readable)
          {
            logg("SOCK: %d %.*s",sc->readable,sc->readable,sc->readbuf);
            sockc_zread(sc,sockc_write(sc,sc->readbuf,sc->readable));
          }
          if(sc->state == SOCKC_CLOSED) logg("SOCK CLOSED %s",c->to->hexname);
        }
        if(util_cmp(c->type,"chat") == 0 && ext_chat(c)) while((p = chat_pop(chat)))
        {
          if(util_cmp(lob_get_str(p,"type"),"state") == 0)
          {
            logg("%s joined",lob_get_str(p,"text"));
          }
          if(util_cmp(lob_get_str(p,"type"),"chat") == 0)
          {
            logg("%s> %s",lob_get_str(chat_participant(chat,lob_get_str(p,"from")),"text"),lob_get_str(p,"text"));
          }
          lob_free(p);
        }
      }

      while((p = chan_pop(c)))
      {
        printf("unhandled channel packet %.*s\n", p->json_len, p->json);      
        lob_free(p);
      }

      DEBUG_PRINTF("channel state %d\n",c->ended);
    }

    if((len = fread(buf,1,255,stdin)))
    {
      buf[len-1] = 0;
      if(strncmp(buf,"/nick ",6) == 0)
      {
        snprintf(nick,16,"%s",buf+6);
        p = chat_message(chat);
        lob_set_str(p,"text",nick);
        chat_join(chat,p);
        logg("");
      }else if(strcmp(buf,"/quit") == 0){
        // TODO test freeing all
        return 0;
      }else if(strcmp(buf,"/debug") == 0){
        util_sys_debugging(-1); // toggle
        logg("");
      }else if(strncmp(buf,"/get ",5) == 0){
        logg("get %s\n",buf+5);
        p = chan_note(admin,NULL);
        lob_set_str(p,"uri",buf+5);
        thtp_req(s,p);
      }else if(strncmp(buf,"/chat ",6) == 0){
        chat_free(chat);
        chat = chat_get(s,buf+6);
        p = chat_message(chat);
        lob_set_str(p,"text",nick);
        chat_join(chat,p);
        logg("joining chat %s %s %s\n",chat->id,lob_get_str(p,"id"),chat->rhash);
      }else if(strlen(buf)){
        // default send as message
        p = chat_message(chat);
        lob_set_str(p,"text",buf);
        chat_send(chat,p);
        logg("");
      }else{
        logg("");
      }
    }
    
    util_sendall(s,sock);
  }

  perror("exiting");
  return 0;
}
