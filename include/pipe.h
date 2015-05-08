#ifndef pipe_h
#define pipe_h
#include <stdint.h>

typedef struct pipe_struct *pipe_t;

#include "mesh.h"
#include "link.h"

struct pipe_struct
{
  char *type;
  char *id;
  uint8_t cloaked, local;
  lob_t path;
  lob_t notify; // who to signal for pipe events
  void *arg; // for use by app/network transport
  pipe_t next; // for transport use
  void (*send)(pipe_t pipe, lob_t packet, link_t link); // deliver this packet via this pipe
};

pipe_t pipe_new(char *type);
pipe_t pipe_free(pipe_t p);


#endif
