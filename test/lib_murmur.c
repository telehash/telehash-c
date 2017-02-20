#include "murmur.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  char hex[9];
  uint8_t *str = (uint8_t*)"foo bar"; // TODO fix this!
  size_t len = strlen((char*)str);

  fail_unless(murmur4(str,len) == 171088983l);
  fail_unless(strcmp("0a329c57",murmur8(str,len,hex)) == 0);
  
  uint8_t hash[4];
  fail_unless(murmur((uint8_t*)"foo bar",7,hash));
  LOG("hex %s",util_hex(hash,4,NULL));
  fail_unless(strcmp("579c320a",util_hex(hash,4,NULL)) == 0);

  return 0;
}

