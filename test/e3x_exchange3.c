#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);
  lob_t idA = e3x_generate();
  fail_unless(idA);

  self3_t selfA = self3_new(idA);
  fail_unless(selfA);

  lob_t idB = e3x_generate();
  lob_t keyB = lob_get_base32(lob_linked(idB),"1a");
  fail_unless(keyB);
  exchange3_t xAB = exchange3_new(selfA, 0x1a, keyB, 1);
  fail_unless(xAB);

  return 0;
}

