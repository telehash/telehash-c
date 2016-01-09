#ifndef pipe_h
#define pipe_h
#include <stdint.h>

#include "mesh.h"
#include "link.h"

struct pipe_struct
{
  lob_t path;
  lob_t links; // who to signal for pipe events
  void *arg; // for use by app/network transport
  pipe_t next; // for transport use
  void (*send)(pipe_t pipe, lob_t packet, link_t link); // deliver this packet via this pipe

  char *type;
  char *id;
  uint8_t down:1;
  uint8_t cloaked:1;
  uint8_t local:1;
};

pipe_t pipe_new(char *type);
pipe_t pipe_free(pipe_t p);

// generates notifications to any links using it
pipe_t pipe_sync(pipe_t p, uint8_t down);

// safe wrapper around ->send
pipe_t pipe_send(pipe_t pipe, lob_t packet, link_t link);

#endif
