#ifndef epoch_h
#define epoch_h
#include <stdint.h>

typedef struct epoch_struct *epoch_t;
typedef epoch_t *epochs_t; // arrays

#include "mesh.h"
#include "link.h"

struct epoch_struct
{
  uint8_t bin[16]; // 8 header 8 random body
  char *id; // base32 of bin
  uint8_t type; // bin[0]
  uint8_t txrx; // flag
  uint8_t key[16]; // private key for MAC-AES
  uint32_t win; // current window
  uint32_t chan; // channel base for current window
  uint32_t at; // offset base in current window
  link_t link;
  void *phy; // for use by driver
  uint8_t *buf, len; // filled in by scheduler (tx) or driver (rx)
  
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

// array utilities
epochs_t epochs_add(epochs_t es, epoch_t e);
epochs_t epochs_rem(epochs_t es, epoch_t e);
epoch_t epochs_index(epochs_t es, size_t i);
size_t epochs_len(epochs_t es);

#endif
