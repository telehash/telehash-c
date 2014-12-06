#include "lib.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  lob_t packet;
  packet = lob_new();
  fail_unless(packet);
  lob_free(packet);

  uint8_t buf[1024];
  char *hex = "001d7b2274797065223a2274657374222c22666f6f223a5b22626172225d7d616e792062696e61727921";
  uint8_t len = strlen(hex)/2;
  util_unhex(hex,strlen(hex),buf);
  packet = lob_parse(buf,len);
  fail_unless(packet);
  fail_unless(lob_len(packet));
  fail_unless(packet->head_len == 29);
  fail_unless(packet->body_len == 11);
  fail_unless(util_cmp(lob_get(packet,"type"),"test") == 0);
  fail_unless(util_cmp(lob_get(packet,"foo"),"[\"bar\"]") == 0);
  
  lob_free(packet);
  packet = lob_new();
  lob_set_base32(packet,"32",buf,len);
  fail_unless(lob_get(packet,"32"));
  fail_unless((int)strlen(lob_get(packet,"32")) == (base32_encode_length(len)-1));
  lob_t bin = lob_get_base32(packet,"32");
  fail_unless(bin);
  fail_unless(bin->body_len == len);

  lob_set(packet,"key","value");
  fail_unless(lob_keys(packet) == 2);

  // test sorting
  lob_set(packet,"zz","value");
  lob_set(packet,"a","value");
  lob_set(packet,"z","value");
  lob_sort(packet);
  fail_unless(util_cmp(lob_get_index(packet,0),"32") == 0);
  fail_unless(util_cmp(lob_get_index(packet,2),"a") == 0);
  fail_unless(util_cmp(lob_get_index(packet,4),"key") == 0);
  fail_unless(util_cmp(lob_get_index(packet,6),"z") == 0);
  fail_unless(util_cmp(lob_get_index(packet,8),"zz") == 0);
  lob_free(packet);
  
  // minimal comparison test
  lob_t a = lob_new();
  lob_set(a,"foo","bar");
  lob_t b = lob_new();
  lob_set(b,"foo","bar");
  fail_unless(lob_cmp(a,b) == 0);
  lob_set(b,"bar","foo");
  fail_unless(lob_cmp(a,b) != 0);

  // test cloaking
  lob_t raw = lob_new();
  lob_set(raw,"test","cloaked");
  char hexed[128];
  util_hex(lob_cloak(raw,4),lob_len(raw)+(8*4),hexed);
//  fail_unless(util_cmp(hexed,"67c6697351ff4aec595edb2689e6bc4f43bd49ffe739b84a8b941dda34337565697763657a74706e34726475777a636d6a717865") == 0);
  uint8_t unhexed[64];
  lob_t raw2 = lob_decloak(util_unhex(hexed,strlen(hexed),unhexed),strlen(hexed)/2);
  fail_unless(!raw2);
//  fail_unless(util_cmp(lob_get(raw2,"test"),"cloaked") == 0);

  lob_t list = lob_new();
  lob_t item = lob_new();
  fail_unless(lob_push(list,item));
//  fail_unless(lob_pop(list) == item);
  fail_unless(lob_unshift(list,item));
//  fail_unless(lob_shift(list) == item);
  fail_unless(lob_push(list,item));
  fail_unless(lob_splice(list,item));
  
  return 0;
}

