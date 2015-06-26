#ifndef epoch_h
#define epoch_h
#include <stdint.h>

typedef struct epoch_struct *epoch_t;

#include "mesh.h"
#include "link.h"

struct epoch_struct
{
  uint8_t bin[16];
  char *id;
  uint8_t type;
  uint8_t pad[32];
  link_t link;
  void *phy; // for use by driver
  
  struct epoch_struct *next;
};

epoch_t epoch_new(epoch_t next);
epoch_t epoch_free(epoch_t e);
epoch_t epoch_reset(epoch_t e);
char *epoch_id(epoch_t e); // friendly base32 encoding
epoch_t epoch_import(epoch_t e, char *eid); // also resets

// scheduling stuff
epoch_t epoch_sync(epoch_t e, uint32_t window, uint64_t at); // sync point for given window
uint64_t epoch_knock(epoch_t e, uint64_t from); // when is next knock, returns 0 if == from

// phy utilities
uint32_t epoch_chan(epoch_t e, uint64_t at, uint8_t chans); // which channel to use at this time


#endif
