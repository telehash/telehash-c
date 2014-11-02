#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  lob_t opts = lob_new();
  fail_unless(e3x_init(opts) == 0);
  fail_unless(!lob_get(opts,"err"));
  fail_unless(!e3x_err());
  
  return 0;
}

