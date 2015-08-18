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
  chunk_t chunks; // actual chunk encoding
  uint8_t len:7; // <= 64
  enum {TX, RX} dir:1;
  enum {NONE, READY, DONE, ERR} state:2;
};

// individual epoch+medium state data, goal to keep <64b each on 32bit
struct epoch_struct
{
  uint8_t secret[32];
  uint64_t base; // microsecond of window 0 start
  knock_t knock; // only exists when active
  epoch_t next; // for epochs_* list utils
  void *device; // used by radio device driver
  epoch_t bound; // radio keeps a list of its bound epochs 
  uint32_t busy; // microseconds to tx/rx, set by driver
  uint8_t chans; // number of total channels, set by driver
  uint8_t radio; // radio device id based on radio_devices[]
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


#endif
