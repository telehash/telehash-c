#include "lob.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  lob_t packet;
  packet = lob_new();
  fail_unless(packet);

  uint8_t buf[1024];
  char *hex = "001d7b2274797065223a2274657374222c22666f6f223a5b22626172225d7d616e792062696e61727921";
  util_unhex(hex,strlen(hex),buf);
  packet = lob_parse(buf,strlen(hex)/2);
  fail_unless(packet);
  fail_unless(lob_len(packet));
  fail_unless(packet->head_len == 29);
  fail_unless(packet->body_len == 11);
  fail_unless(util_cmp(lob_get(packet,"type"),"test") == 0);
  fail_unless(util_cmp(lob_get(packet,"foo"),"[\"bar\"]") == 0);

  return 0;
}

