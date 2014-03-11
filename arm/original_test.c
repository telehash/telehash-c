#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "platform.h"

#include "ecc.h"
#include "aes.h"
#include "sha256.h"
#include "sha1.h"
#include "hmac.h"
#include "js0n.h"

/* scratch
typedef struct sockaddr_in {};
#define time_t long
#define time(x) millis()

*/

int RNG(uint8_t *p_dest, unsigned p_size)
{
  while(p_size--)
  {
    *p_dest = (uint8_t)random();
    p_dest++;
  }
  return 1;
}

void vli_print(uint8_t *p_vli, unsigned int p_size)
{
    while(p_size)
    {
      char hex[8];
      sprintf(hex,"%02X ", (unsigned)p_vli[p_size - 1]);
      printf("%s",hex);
      --p_size;
    }
}


void setup() {
// Any setup required goes here.
}

int ecc_test(int loops)
{
    int i;
    
    uint8_t l_private1[ECC_BYTES];
    uint8_t l_private2[ECC_BYTES];
    
    uint8_t l_public1[ECC_BYTES *2];
    uint8_t l_public2[ECC_BYTES *2];
    
    uint8_t l_secret1[ECC_BYTES];
    uint8_t l_secret2[ECC_BYTES];

    ecc_set_rng(&RNG);
    
    printf("Testing random private key pairs\n");

    for(i=0; i<loops; ++i)
    {
        long a = millis();
        ecc_make_key(l_public1, l_private1);
        printf("%lu",millis()-a);
        printf(" gen\n");
        a = millis();
        ecc_make_key(l_public2, l_private2);
        printf("%lu",millis()-a);
        printf(" gen\n");


        a = millis();
        if(!ecdh_shared_secret(l_public2, l_private1, l_secret1))
        {
            printf("shared_secret() failed (1)\n");
            return 1;
        }
        printf("%lu",millis()-a);
        printf(" dh\n");

        a = millis();
        if(!ecdh_shared_secret(l_public1, l_private2, l_secret2))
        {
            printf("shared_secret() failed (2)\n");
            return 1;
        }
        printf("%lu",millis()-a);
        printf(" dh\n");
        
        if(memcmp(l_secret1, l_secret2, sizeof(l_secret1)) != 0)
        {
            printf("Shared secrets are not identical!\n");
            printf("Shared secret 1 = ");
            vli_print(l_secret1, ECC_BYTES);
            printf("\n");
            printf("Shared secret 2 = ");
            vli_print(l_secret2, ECC_BYTES);
            printf("\n");
            printf("Private key 1 = ");
            vli_print(l_private1, ECC_BYTES);
            printf("\n");
            printf("Private key 2 = ");
            vli_print(l_private2, ECC_BYTES);
            printf("\n");
        }
    }
    printf("\n");
    
    return 0;
}

int aes_test()
{
  aes_context ctx;
  unsigned char key[16], data[6],iv[16],block[16];
  size_t off = 0;
  memcpy(key,"1234567812345678",16);
  memset(iv,0,16);
  memcpy(data,"foobar",6);
  aes_setkey_enc(&ctx,(unsigned char*)key,16);
  aes_crypt_ctr(&ctx,6,&off,iv,block,data,data);
  printf("aes ");
  vli_print(data,6);
  printf("\n");
}

int sha256_test()
{
  unsigned char hash[SHA256_HASH_BYTES];
  sha256((uint8_t (*)[32])hash,"foo",3);
  printf("sha256 ");
  vli_print(hash,SHA256_HASH_BYTES);
  printf("\n");
}

int sha1_test()
{
  unsigned char hash[SHA1_HASH_BYTES];
  sha1(hash,"foo",3);
  printf("sha1 ");
  vli_print(hash,SHA1_HASH_BYTES);
  printf("\n");
}

int hmac_test()
{
  unsigned char hmac[HMAC_SHA1_BYTES];
  hmac_sha1(hmac,"foo",3,"bar",3);
  printf("hmac ");
  vli_print(hmac,HMAC_SHA1_BYTES);
  printf("\n");
}

int main(void) {
  long start = millis();
  aes_test();
  sha1_test();
  hmac_test();
  sha256_test();
  printf("Time to complete aes_test(), sha1_test(), hmac_test() & sha256_test(): %lu\n",millis() - start);
  ecc_test(5);
  printf("\n");
  return 0;
}

