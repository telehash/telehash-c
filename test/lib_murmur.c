#include "murmur.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  char hex[9];
  uint8_t *str = (uint8_t*)"foo bar";
  uint32_t len = strlen((char*)str);

  fail_unless(murmur4(str,len) == 171088983l);
  fail_unless(strcmp("0a329c57",murmur8(str,len,hex)) == 0);

  return 0;
}

