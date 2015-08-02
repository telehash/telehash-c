#ifndef epoch_h
#define epoch_h
#include <stdint.h>

typedef struct epoch_struct *epoch_t;
typedef struct medium_struct *medium_t;
typedef struct knock_struct *knock_t;

#include "mesh.h"
#include "link.h"

// knock state holder when sending/receiving
struct knock_struct
{
  uint32_t win; // current window id
  uint32_t chan; // current channel (< med->chans)
  uint64_t start, stop; // microsecond exact start/stop time
  uint8_t *buf; // filled in by scheduler (tx) or driver (rx)
  uint8_t len; // <= 64
};

// individual epoch+medium state data, goal to keep <64b each
struct epoch_struct
{
  uint64_t base; // microsecond of window 0 start
  uint8_t secret[32];
  knock_t active; // only when scheduled
  epoch_t next; // for epochs_* list utils
  void *phy; // used by medium PHY driver
  uint32_t busy; // microseconds to tx/rx, set by PHY driver
  uint8_t chans; // number of total channels, set by PHY driver
  uint8_t type; // medium type
  uint8_t tx; // boolean if is a tx or rx
};


epoch_t epoch_new(uint8_t type, uint8_t tx);
epoch_t epoch_free(epoch_t e);
epoch_t epoch_base(epoch_t e, uint32_t window, uint64_t at); // sync point for given window

// reset knock to next window, 0 cleans out
epoch_t epoch_knock(epoch_t e, uint64_t at);

// simple array utilities
epoch_t epochs_add(epoch_t es, epoch_t e);
epoch_t epochs_rem(epoch_t es, epoch_t e);
size_t epochs_len(epoch_t es);
epoch_t epochs_free(epoch_t es);

#endif
