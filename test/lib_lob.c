#include "lib.h"
#include "util.h"
#include "e3x.h"
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
  fail_unless(strlen(lob_get(packet,"32")) == (base32_encode_length(len)-1));
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

  // lots of basic list testing
  lob_t list = lob_new();
  lob_t item = lob_new();
  fail_unless(lob_push(list,item));
  fail_unless(lob_pop(list) == item);
  list = item->next;
  fail_unless((list = lob_unshift(list,item)));
  fail_unless(lob_shift(list) == item);
  list = item->next;
  fail_unless(lob_push(list,item));
  fail_unless(list->next == item);
  lob_t insert = lob_new();
  fail_unless(lob_insert(list,list,insert));
  fail_unless(list->next == insert);
  fail_unless(insert->next == item);
  fail_unless(lob_splice(list,insert));
  fail_unless(list->next == item);

  lob_t array = lob_array(list);
  fail_unless(array);
  fail_unless(util_cmp(lob_json(array),"[,]") == 0);

  fail_unless(lob_freeall(list) == NULL);

  // simple index testing
  lob_t index = lob_new();
  lob_t c1 = lob_new();
  lob_set(c1,"id","c1");
  lob_push(index,c1);
  lob_t c2 = lob_new();
  lob_set(c2,"id","c2");
  lob_push(index,c2);
  fail_unless(lob_match(index,"id","c1") == c1);
  fail_unless(lob_match(index,"id","c2") == c2);
  
  float f = 42.42;
  lob_t ft = lob_new();
  lob_head(ft,(uint8_t*)"{\"foo\":42.42}",13);
  fail_unless(lob_get_float(ft,"foo") == f);
  lob_set_float(ft,"bar2",f,2);
  fail_unless(lob_get_float(ft,"bar2") == f);
  lob_set_float(ft,"bar1",f,1);
  fail_unless(lob_get_cmp(ft,"bar1","42.4") == 0);
  lob_set_float(ft,"bar0",f,0);
  fail_unless(lob_get_int(ft,"bar0") == 42);
  LOG("floats %s",lob_json(ft));

  return 0;
}

