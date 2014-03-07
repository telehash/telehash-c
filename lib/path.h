#ifndef path_h
#define path_h

#include <sys/socket.h>
#include <arpa/inet.h>

typedef struct path_struct
{
  char type[12];
  unsigned char *json;
  char *id;
  char ip[46];
  int port;
  struct	sockaddr_in sa;
  unsigned long atIn, atOut;
} *path_t;

path_t path_new(char *type);
void path_free(path_t p);

// create a new path from json
path_t path_parse(unsigned char *json, int len);

// return json for a path
unsigned char *path_json(path_t p);

// these set and/or return path values
char *path_id(path_t p, char *id);
char *path_ip(path_t p, char *ip);
char *path_http(path_t p, char *http);
int path_port(path_t p, int port);

// inits ip/port from .sa
void path_sa(path_t p);

// compare to see if they're the same
int path_match(path_t p1, path_t p2);

// tell if the path is alive (recently active), 1 = yes, 0 = no
int path_alive(path_t p);

#endif