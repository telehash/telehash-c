#include "chacha.h"
#include "util.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  uint8_t key[32], nonce[8], test[9];
  char hex[65];

  memset(key,0,32);
  memset(nonce,0,8);
  memset(test,0,9);

  fail_unless(chacha20(key,nonce,test,9));
  fail_unless(util_cmp(util_hex(test,9,hex),"76b8e0ada0f13d9040") == 0);
  fail_unless(chacha20(key,nonce,test,9));
  fail_unless(util_cmp(util_hex(test,9,hex),"000000000000000000") == 0);

  memset(key,0xff,32);
  memset(nonce,0xff,8);
  memset(test,0xff,9);

  fail_unless(chacha20(key,nonce,test,9));
  fail_unless(util_cmp(util_hex(test,9,hex),"2640c09431912f4abd") == 0);
  fail_unless(chacha20(key,nonce,test,9));
  fail_unless(util_cmp(util_hex(test,9,hex),"ffffffffffffffffff") == 0);


  return 0;
}

