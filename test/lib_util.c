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

  // test the constant time memcmp
  uint8_t buf1[] = {0x03};
  uint8_t buf2[] = {0x01, 0x02};
  uint8_t buf3[] = {0x01, 0x02};
  uint8_t buf4[] = {0x01, 0x02, 0x03};
  uint8_t buf5[] = {0x01, 0x04, 0x03};
  uint8_t buf6[] = {0x02, 0x02, 0x03};
  uint8_t buf7[] = {0x01, 0x02, 0x04};

  fail_unless(util_ct_memcmp(buf2, buf2, sizeof(buf2)) == 0);
  fail_unless(util_ct_memcmp(buf2, buf3, sizeof(buf2)) == 0);
  fail_unless(util_ct_memcmp(buf3, buf4, sizeof(buf3)) == 0);
  fail_unless(util_ct_memcmp(buf3, buf4, sizeof(buf3)) == 0);

  // Negative test cases
  fail_unless(util_ct_memcmp(buf1, buf2, sizeof(buf1)) != 0);
  fail_unless(util_ct_memcmp(buf4, buf5, sizeof(buf5)) != 0);
  fail_unless(util_ct_memcmp(buf4, buf6, sizeof(buf5)) != 0);
  fail_unless(util_ct_memcmp(buf4, buf7, sizeof(buf5)) != 0);

  return 0;
}
