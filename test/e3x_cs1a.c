#include "e3x.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  lob_t opts = lob_new();
  fail_unless(e3x_init(opts) == 0);
  fail_unless(!e3x_err());

  cipher3_t cs = cipher3_set(0x1a,NULL);
  fail_unless(cs);
  cs = cipher3_set(0,"1a");
  fail_unless(cs);
  fail_unless(cs->id == CS_1a);
  
  uint8_t buf[32];
  fail_unless(e3x_rand(buf,32));

  char hex[65];
  util_hex(e3x_hash((uint8_t*)"foo",3,buf),32,hex);
  fail_unless(strcmp(hex,"2c26b46b68ffc68ff99b453c1d30413413422d706483bfa0f98a5e886266e7ae") == 0);

  lob_t id = e3x_generate();
  fail_unless(id);
  fail_unless(lob_get(id,"1a"));
  fail_unless(lob_linked(id));
  fail_unless(lob_get(lob_linked(id),"1a"));

  return 0;
}

