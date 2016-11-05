#include "telehash.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  uint8_t hash[64];
  sha256((uint8_t*)"foo", 3, hash, 0);
  char hex[256];
  util_hex(hash,32,hex);
  fail_unless(strcmp(hex,"2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae") == 0);

  char *salt = "salt123";
  char *ikm = "initialKeyingMaterial";
  char *info = "info";
  fail_unless(0 == hkdf_sha256((uint8_t*)salt, strlen(salt), (uint8_t*)ikm, strlen(ikm), (uint8_t*)info, strlen(info), hash, 42));
  util_hex(hash,42,hex);
  fail_unless(strcmp(hex,"8dfce091422811f95e509909ddab00bdb60668837e0400ec01170d8216fbe501bec33b8762338e927fa1") == 0);

  return 0;
}
