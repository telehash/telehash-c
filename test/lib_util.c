#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  char hex[32];
  uint8_t *str = (uint8_t*)"foo bar";
  uint32_t len = strlen((char*)str);

  fail_unless(strcmp("666f6f20626172",util_hex(str,len,hex)) == 0);

  return 0;
}

