#include <telehash.h>
#include <dew.h>

#include "./lob.c"
#include "./transform.c"
#include "./mesh.c"
#include "./link.c"

// adds in types/singletons for telehash stuff
dew_t telehash_dew(dew_t stack, bool mesh)
{
  // add libs by default
  // - lob type
  stack = throwback_lib_lob(stack);
  // - base32/64
  // - sha256
  // - chacha/aes128
  // - murmur

  // add meshing/pk
  if(mesh)
  {
    // - jwt
    // - mesh type
    // - link type
  }

  return stack;
}
