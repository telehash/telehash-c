#ifndef epoch_h
#define epoch_h
#include <stdint.h>

typedef struct epoch_struct *epoch_t;
typedef struct knock_struct *knock_t;

#include "mesh.h"
#include "link.h"

// knock state holder when sending/receiving
struct knock_struct
{
  knock_t next; // for temporary lists
  epoch_t epoch; // so knocks can be passed around directly
  uint32_t win; // current window id
  uint32_t chan; // current channel (< med->chans)
  uint64_t start, stop; // microsecond exact start/stop time
  uint8_t *buf; // filled in by scheduler (tx) or driver (rx)
  uint8_t len; // <= 64
  uint8_t tx; // boolean if is a tx or rx
};

// individual epoch+medium state data, goal to keep <64b each
struct epoch_struct
{
  uint64_t base; // microsecond of window 0 start
  uint8_t secret[32];
  knock_t knock; // only when scheduled
  epoch_t next; // for epochs_* list utils
  void *driver; // used by medium driver
  uint32_t busy; // microseconds to tx/rx, set by driver
  uint8_t medium[6];
  uint8_t chans; // number of total channels, set by driver
};


epoch_t epoch_new(mesh_t m, uint8_t medium[6]);
epoch_t epoch_free(epoch_t e);

// friendly base32 of this epoch's medium, static/reused pointer per call
char *epoch_medium(epoch_t e);

// sync point for given window
epoch_t epoch_base(epoch_t e, uint32_t window, uint64_t at);

// reset active knock to next window, 0 cleans out, guarantees an e->knock or returns NULL
epoch_t epoch_knock(epoch_t e, uint64_t at);

// simple array utilities
epoch_t epochs_add(epoch_t es, epoch_t e);
epoch_t epochs_rem(epoch_t es, epoch_t e);
size_t epochs_len(epoch_t es);
epoch_t epochs_free(epoch_t es);

///////////////////
// every epoch needs a medium driver that has to be aware of the epoch's lifecycle events
typedef struct epoch_driver_struct
{
  // return energy cost, or 0 if unknown medium
  uint32_t (*get)(mesh_t mesh, uint8_t medium[6]);

  // used to initialize all new epochs, add medium scheduling time/cost and channels
  void* (*init)(mesh_t mesh, epoch_t e, uint8_t medium[6]);

  epoch_t (*free)(mesh_t mesh, epoch_t e);

} *epoch_driver_t;

#define EPOCH_DRIVERS_MAX 3
extern epoch_driver_t epoch_drivers[]; // all created

// add/set a new medium driver
epoch_driver_t epoch_driver(epoch_driver_t driver);

#endif
