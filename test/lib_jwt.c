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
  
  // test signing
  fail_unless(e3x_init(NULL) == 0);

  if(jwt_alg("HS256"))
  {
    lob_t hs256 = lob_new();
    lob_set(hs256,"alg","HS256");
    lob_set(hs256,"typ","JWT");
    lob_t hsp = lob_new();
    lob_set_int(hsp,"sub",42);
    lob_link(hs256,hsp);
    // set hmac secret on token body
    lob_body(hs256,(uint8_t*)"secret",6);
    fail_unless(jwt_sign(hs256,NULL));
    fail_unless(hsp->body_len == 32);
    printf("signed JWT: %s\n",jwt_encode(hs256));
    lob_body(hs256,(uint8_t*)"secret",6);
    fail_unless(jwt_verify(hs256,NULL));
  }

  // test real signing using generated keys
  lob_t id = e3x_generate();
  e3x_self_t self = e3x_self_new(id,NULL);
  fail_unless(self);

  if(jwt_alg("ES160"))
  {
    lob_t es160 = lob_new();
    lob_set(es160,"alg","ES160");
    lob_set(es160,"typ","JWT");
    lob_t esp = lob_new();
    lob_set_int(esp,"sub",42);
    lob_link(es160,esp);
    fail_unless(jwt_sign(es160,self));
    fail_unless(esp->body_len == 40);
    printf("signed JWT: %s\n",jwt_encode(es160));

    lob_t key = lob_get_base32(lob_linked(id),"1a");
    e3x_exchange_t x = e3x_exchange_new(self, 0x1a, key);
    fail_unless(x);
    fail_unless(jwt_verify(es160,x));
  }

  if(jwt_alg("RS256"))
  {
    lob_t test = lob_new();
    lob_set(test,"alg","RS256");
    lob_set(test,"typ","JWT");
    lob_t testp = lob_new();
    lob_set_int(testp,"sub",42);
    lob_link(test,testp);
    fail_unless(jwt_sign(test,self));
    fail_unless(testp->body_len == 256);
    printf("signed JWT: %s\n",jwt_encode(test));

    lob_t key = lob_get_base32(lob_linked(id),"2a");
    e3x_exchange_t x = e3x_exchange_new(self, 0x2a, key);
    fail_unless(x);
    fail_unless(jwt_verify(test,x));
  }


  return 0;
}

