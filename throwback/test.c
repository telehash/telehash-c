#include "dew.h"
#include "../test/unit_test.h"

int main(int argc, char **argv)
{
  dew_t stack = dew_lib_js10(NULL);
  fail_unless(stack);
  
  return 0;
}

