#include "uri.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  uri_t uri;

  fail_unless((uri = uri_new("foo://bar",NULL)));
  fail_unless(uri_protocol(uri,"bar"));
  fail_unless(util_cmp(uri_encode(uri),"bar://bar") == 0);
  fail_unless(uri_canonical(uri,"canon:42"));
  fail_unless(uri_token(uri,"token"));
  fail_unless(uri_token(uri,NULL));
  fail_unless(uri_session(uri,"sid"));
  fail_unless(util_cmp(uri_encode(uri),"bar://canon:42/sid") == 0);
  fail_unless(uri_port(uri,69));
  fail_unless(uri_user(uri,"test.aa"));
  fail_unless(util_cmp(uri_encode(uri),"bar://test.aa@canon:69/sid") == 0);

  fail_unless(uri_free(uri) == NULL);

  return 0;
}

