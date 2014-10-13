#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  fail_unless(e3x_init(NULL) == 0);
  lob_t id = e3x_generate();
  fail_unless(id);

  self3_t self = self3_new(id);
  fail_unless(self);

  exchange3_t x = exchange3_new(self, NULL, 0);
  fail_unless(x);

  return 0;
}

