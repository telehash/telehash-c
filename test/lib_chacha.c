#include "chacha.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  uint8_t key[32], nonce[8], test[4];

  memset(key,0,32);
  memset(nonce,0,8);
  memset(test,0,4);

  fail_unless(chacha20(key,nonce,test,4));
  fail_unless(test[0] == 0x76 && test[1] == 0xb8 && test[2] == 0xe0 && test[3] == 0xad);

  return 0;
}

