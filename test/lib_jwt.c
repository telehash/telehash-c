#include "jwt.h"
#include "util.h"
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
  fail_unless(payload->body_len == 32);
  
  char jwt2[] = "eyJ0eXAiOiJKV1QiLCJhbGciOiJSUzI1NiIsImtpZCI6ImlkIn0.eyJpc3MiOiJ0ZXN0Iiwic3ViIjoiZXhwZWN0IiwiYXVkIjoieW91IiwiYXpwIjoibWUiLCJpYXQiOjE0MjEwMjk5MzQsInNjb3BlIjoidGVzdCIsImV4cCI6MTczNjM4OTkzNH0.aHORuwSmjEl5UOZaQ-eDggDjzMBc7i5pSZCYEDT5mpP7S9c3h_I-6pGaD9W4xu79VTidjsyspS6P4FZUWGkmMRfcXPr0Uv-SbbKzD5E_T-xkp5SxL3AMV9Up9BcsM6hZU_tHxl5XBmM8IZztPV2asL77flQtrIvsc7DABw9r9SQ";
  head = jwt_decode(jwt2, 0);
  fail_unless(head);
  fail_unless(lob_get(head,"typ"));
  fail_unless(lob_get(head,"alg"));
  payload = lob_linked(head);
  fail_unless(payload);
  fail_unless(lob_get(payload,"sub"));
  fail_unless(lob_get(payload,"scope"));
  fail_unless(payload->body_len == 128);

  char orig[] = "eyJ0eXAiOiJKV1QiLCJhbGciOiJSUzI1NiIsImtpZCI6ImlkIn0.eyJpc3MiOiJ0ZXN0Iiwic3ViIjoiZXhwZWN0IiwiYXVkIjoieW91IiwiYXpwIjoibWUiLCJpYXQiOjE0MjEwMjk5MzQsInNjb3BlIjoidGVzdCIsImV4cCI6MTczNjM4OTkzNH0.aHORuwSmjEl5UOZaQ-eDggDjzMBc7i5pSZCYEDT5mpP7S9c3h_I-6pGaD9W4xu79VTidjsyspS6P4FZUWGkmMRfcXPr0Uv-SbbKzD5E_T-xkp5SxL3AMV9Up9BcsM6hZU_tHxl5XBmM8IZztPV2asL77flQtrIvsc7DABw9r9SQ";
  char *enc = jwt_encode(head);
  fail_unless(enc);
  fail_unless(util_cmp(orig,enc) == 0);
  
  return 0;
}

