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
// similar to singleton pattern, registers a transform
// NOTE: create and process cannot modify stack, process args1/2 swap for update vs final calls
dew_t dew_set_xform(dew_t stack, char *name, dew_fun_t create, dew_fun2_t process, dew_free_t free, void 
  *arg);


#endif 

