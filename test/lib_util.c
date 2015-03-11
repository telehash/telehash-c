#include <unistd.h>
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  char hex[32];
  uint8_t *str = (uint8_t*)"foo bar";
  uint32_t len = strlen((char*)str);

  fail_unless(strcmp("666f6f20626172",util_hex(str,len,hex)) == 0);

  uint64_t at = util_at();
  fail_unless(at > 0);
  sleep(1);
  fail_unless(util_since(at));

  return 0;
}

