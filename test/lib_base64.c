#include "base64.h"
#include "e3x.h"
#include "unit_test.h"

/*
BASE64("") = ""
   BASE64("f") = "Zg=="
   BASE64("fo") = "Zm8="
   BASE64("foo") = "Zm9v"
   BASE64("foob") = "Zm9vYg=="
   BASE64("fooba") = "Zm9vYmE="
   BASE64("foobar") = "Zm9vYmFy"
*/

int main(int argc, char **argv)
{
  char *eout = malloc(base64_encode_length(7));
  uint8_t *dout = malloc(base64_decode_length(8));
  size_t len;

  len = base64_encode((const uint8_t *)"foobar", 6, eout);
  fail_unless(len == 8);
  fail_unless(strcmp(eout,"Zm9vYmFy") == 0);
  len = base64_decode(eout,0,dout);
  fail_unless(len == 6);
  fail_unless(strncmp((char*)dout,"foobar",6) == 0);

  char bfix[] = "eyJzdWIiOjEyMzQ1Njc4OTAsIm5hbWUiOiJKb2huIERvZSIsImFkbWluIjp0cnVlfQ";
  char jfix[] = "{\"sub\":1234567890,\"name\":\"John Doe\",\"admin\":true}";
  char *jtest = malloc(base64_decode_length(strlen(bfix)));
  len = base64_decode(bfix,0,(uint8_t*)jtest);
  fail_unless(len == strlen(jfix));
  fail_unless(strcmp(jtest,jfix) == 0);
  char *btest = malloc(base64_encode_length(strlen(jfix)));
  len = base64_encode((uint8_t*)jfix,strlen(jfix),btest);
//  printf("len %d %d %s",len,strlen(bfix),btest);
  fail_unless(len == strlen(bfix));
  fail_unless(strncmp(btest,bfix,len) == 0);

  e3x_init(NULL); // random seed
  uint8_t *rand = malloc(32);
  char *rtest = malloc(base64_encode_length(32));
  uint8_t *rand2 = malloc(32);
  char *rtest2 = malloc(base64_encode_length(32));
  int i;
  for(i = 1;i<=32;i++)
  {
    e3x_rand(rand, i);
    e3x_rand(rand2, i);
    base64_encode(rand,i,rtest);
    base64_decode(rtest,0,rand2);
    base64_encode(rand2,i,rtest2);
//    printf("%s\n%s\n",rtest,rtest2);
    fail_unless(memcmp(rand,rand2,i) == 0);
  }
  
  return 0;
}

