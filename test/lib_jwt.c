#include "jwt.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  char jwt[] = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOjEyMzQ1Njc4OTAsIm5hbWUiOiJKb2huIERvZSIsImFkbWluIjp0cnVlfQ.eoaDVGTClRdfxUZXiPs3f8FmJDkDE_VCQFXqKxpLsts";

  lob_t head = jwt_decode(jwt, 0);
  fail_unless(head);
  fail_unless(lob_get(head,"typ"));
  fail_unless(lob_get(head,"alg"));
  lob_t payload = lob_linked(head);
  fail_unless(payload);
  fail_unless(lob_get(payload,"sub"));
  fail_unless(lob_get(payload,"name"));
  fail_unless(payload->body_len == 31);
  

  return 0;
}

