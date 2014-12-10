#include "uri.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  uri_t uri;

  fail_unless((uri = uri_new("foo://bar",NULL)));
  fail_unless(util_cmp(uri->protocol,"foo") == 0);
  fail_unless(uri_protocol(uri,"bar"));
  fail_unless(util_cmp(uri->protocol,"bar") == 0);
  fail_unless(util_cmp(uri_encode(uri),"bar://bar/") == 0);
  fail_unless(uri_canonical(uri,"canon:42"));
  fail_unless(uri_token(uri,"token"));
  fail_unless(uri_token(uri,NULL));
  fail_unless(uri_session(uri,"sid"));
  fail_unless(util_cmp(uri_encode(uri),"bar://canon:42/sid") == 0);
  fail_unless(uri_port(uri,69));
  fail_unless(uri_user(uri,"test.aa"));
  fail_unless(util_cmp(uri_encode(uri),"bar://test.aa@canon:69/sid") == 0);
  fail_unless(uri_free(uri) == NULL);

  // test key querystring handling
  fail_unless((uri = uri_new("foo://bar/?1a=foo&2a=bar",NULL)));
  fail_unless(util_cmp(uri->protocol,"foo") == 0);
  fail_unless(uri->keys);
  fail_unless(util_cmp(lob_get(uri->keys,"1a"),"foo") == 0);
  fail_unless(util_cmp(lob_get(uri->keys,"2a"),"bar") == 0);
  lob_set(uri->keys,"0a","test");
  fail_unless(util_cmp(uri_encode(uri),"foo://bar/?0a=test&1a=foo&2a=bar") == 0);

  // load test
  int i;
  for(i=0;i<100;i++)
  {
    uri = uri_new("link://127.0.0.1:52402/?1a=ao6yg5zcxxrptsmbyefd74qkwq6tudxkse", NULL);
    fail_unless(uri_encode(uri));
    uri_free(uri);
  }

  return 0;
}

