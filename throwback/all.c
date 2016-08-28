#include "throwback.h"

#include "./lob.c"
#include "./xform.c"

// adds in types/singletons for telehash stuff
dew_t telehash_dew(dew_t stack, bool mesh)
{
  // add libs by default
  // - lob type
  stack = dew_lib_lob(stack);
  // - base32/64
  // - sha256
  // - chacha/aes128
  // - murmur
  stack = dew_lib_xform(stack);

  // add meshing/pk
  if(mesh)
  {
    // - jwt
    // - mesh type
    // - link type
  }

  return stack;
}
