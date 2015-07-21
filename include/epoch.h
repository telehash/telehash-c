#ifndef epoch_h
#define epoch_h
#include <stdint.h>

typedef struct epoch_struct *epoch_t;
typedef struct knock_struct *knock_t;

#include "mesh.h"
#include "link.h"

struct epoch_struct
{
  uint8_t bin[16]; // 8 header 8 random body
  uint8_t iv[8]; // base IV for MAC-AES
  uint64_t bday; // microsecond of window 0 start
  epoch_t next; // for epochs_* interface, lists
  uint32_t busy; // microseconds to tx/rx, set by external/phy
  uint8_t chans; // number of total channels, set by external/phy
};

struct knock_struct
{
  epoch_t e;
  uint32_t win; // current window id
  uint32_t chan; // current channel (< e->chans)
  uint64_t start, stop; // microsecond exact start/stop time
  uint8_t *buf; // filled in by scheduler (tx) or driver (rx)
  knock_t next;
  uint8_t tx; // boolean if is a tx or rx
  uint8_t len; // <= 64
};

epoch_t epoch_new(char *id);
epoch_t epoch_free(epoch_t e);
epoch_t epoch_reset(epoch_t e);
char *epoch_id(epoch_t e); // friendly base32 encoding
epoch_t epoch_import(epoch_t e, char *id, char *body); // also resets, body optional

// scheduling stuff
epoch_t epoch_sync(epoch_t e, uint32_t window, uint64_t at); // sync point for given window
knock_t knock_new(uint8_t tx); // generate a new/blank knock
knock_t epoch_knock(epoch_t e, knock_t k, uint64_t from); // init knock to current window of from
knock_t knock_free(knock_t k); // frees

// phy utilities
epoch_t epoch_busy(epoch_t e, uint32_t us); // microseconds for how long the action takes
epoch_t epoch_chans(epoch_t e, uint8_t chans); // number of possible channels

// simple array utilities
epoch_t epochs_add(epoch_t es, epoch_t e);
epoch_t epochs_rem(epoch_t es, epoch_t e);
size_t epochs_len(epoch_t es);
epoch_t epochs_free(epoch_t es);

#endif
