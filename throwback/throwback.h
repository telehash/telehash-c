#ifndef throwback_h
#define throwback_h

#include "telehash.h"
#include "dew.h"

// all.c
extern dew_t telehash_dew(dew_t stack, bool mesh);

// lob.c
#define TYPEOF_LOB TYPEOF_EXT1
dew_t dew_set_lob(dew_t d, lob_t lob);
lob_t dew_get_lob(dew_t d, bool own);

// xform.c
#define TYPEOF_XFORM TYPEOF_EXT2


#endif 

