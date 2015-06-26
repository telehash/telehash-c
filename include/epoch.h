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
  uint8_t *type;
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
uint64_t epoch_knock(epoch_t e, uint64_t from); // when is next knock, returns 0 if == from


#endif
