#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  lob_t uri;

  fail_unless((uri = util_uri_parse("foo://bar:1?cs1a=foo")));
  fail_unless(util_cmp(lob_get(uri,"protocol"),"foo") == 0);
  fail_unless(util_cmp(lob_get(uri,"host"),"bar:1") == 0);
  fail_unless(util_cmp(lob_get(uri,"hostname"),"bar") == 0);
  fail_unless(lob_get_uint(uri,"port") == 1);
  lob_t keys = util_uri_keys(uri);
  fail_unless(keys);
  fail_unless(util_cmp(lob_get(keys,"1a"),"foo") == 0);

  lob_set(keys,"2a","bar");
  fail_unless(util_uri_add_keys(uri,keys));
  fail_unless(util_cmp(lob_get(lob_linked(uri),"cs2a"),"bar") == 0);

  lob_t path = lob_new();
  lob_set(path,"type","test");
  fail_unless(util_uri_add_path(uri,path));
  fail_unless(lob_get(lob_linked(lob_linked(uri)),"paths"));

  LOG("format: %s",util_uri_format(uri));
  fail_unless(util_cmp(util_uri_format(uri), "foo://bar:1/?cs1a=foo&cs2a=bar&paths=pmrhi6lqmurduitumvzxiit5") == 0);

  return 0;
}

