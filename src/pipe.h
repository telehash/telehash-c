#ifndef pipe_h
#define pipe_h
#include <stdint.h>
#include "lob.h"

typedef struct pipe_struct
{
  char *type;
  char *id;
  lob_t path;
  lob_t notify;
  void *arg; // for use by app/network transport
} *pipe_t;

pipe_t pipe_new(char *type);
void pipe_free(pipe_t p);

/*
typedef struct pipe_struct
{
  char type[12];
  char *json;
  char *id;
  char ip[46];
  uint16_t port;
  uint32_t tin, tout;
  void *arg; // for use by app/network transport
} *pipe_t;

pipe_t pipe_new(char *type);
void pipe_free(pipe_t p);

// create a new pipe from json
pipe_t pipe_parse(char *json, int len);
pipe_t pipe_copy(pipe_t p);

// return json for a pipe
char *pipe_json(pipe_t p);

// these set and/or return pipe values
char *pipe_id(pipe_t p, char *id);
char *pipe_ip(pipe_t p, char *ip);
char *pipe_ip4(pipe_t p, uint32_t ip);
char *pipe_http(pipe_t p, char *http);
uint16_t pipe_port(pipe_t p, uint16_t port);

// inits ip/port from .sa
void pipe_sa(pipe_t p);

// compare to see if they're the same
int pipe_match(pipe_t p1, pipe_t p2);

// tell if the pipe is alive (recently active), 1 = yes, 0 = no
int pipe_alive(pipe_t p);

// tell if pipe is local or public, 1 = local 0 = public
int pipe_local(pipe_t p);
*/
#endif