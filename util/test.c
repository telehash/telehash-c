#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include "util_sys.h"

#include "crypt.h"
#include "ecc.h"
#include "aes.h"
#include "sha256.h"
#include "sha1.h"
#include "js0n.h"
#include "switch.h"


double timer()
{
  struct timeval  tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000 ; 
}

void vli_print(uint8_t *p_vli, unsigned int p_size)
{
  char buf[256];
  printf("%s",util_hex(p_vli,p_size,(unsigned char*)buf));
}


void setup() {
  crypt_init();
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
    
    printf("Testing random private key pairs\n");

    for(i=0; i<loops; ++i)
    {
        double a = timer();
        ecc_make_key(l_public1, l_private1);
        printf("%f",timer()-a);
        printf(" gen\n");
        a = timer();
        ecc_make_key(l_public2, l_private2);
        printf("%f",timer()-a);
        printf(" gen\n");


        a = timer();
        if(!ecdh_shared_secret(l_public2, l_private1, l_secret1))
        {
            printf("shared_secret() failed (1)\n");
            return 1;
        }
        printf("%f",timer()-a);
        printf(" dh\n");

        a = timer();
        if(!ecdh_shared_secret(l_public1, l_private2, l_secret2))
        {
            printf("shared_secret() failed (2)\n");
            return 1;
        }
        printf("%f",timer()-a);
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

void aes_test()
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

void sha256_test()
{
  unsigned char hash[32];
  sha256((unsigned char*)"foo",3,hash,0);
  printf("sha256 ");
  vli_print(hash,32);
  printf("\n");
}

void sha1_test()
{
  unsigned char hash[20];
  sha1((unsigned char*)"foo",3,hash);
  printf("sha1 ");
  vli_print(hash,20);
  printf("\n");
}

void hmac_test()
{
  unsigned char hmac[20];
  sha1_hmac((unsigned char*)"foo",3,(unsigned char*)"bar",3,hmac);
  printf("hmac ");
  vli_print(hmac,20);
  printf("\n");
}

void keygen()
{
  printf("keygen ");

  lob_t keys = lob_new();
  crypt_keygen(0x1a,keys);
  printf("%s",keys->json); //,keys->json_len);
  printf("length=%d\n",keys->json_len);
}

int main() {
  long start = timer();
  keygen();
  aes_test();
  sha1_test();
  hmac_test();
  sha256_test();
  printf("Time taken to complete keygen(), aes_test(), sha1_test(), hmac_test() & sha256_test()=%f\n", timer() - start);
  ecc_test(5);
  DEBUG_PRINTF(("test"));
  printf("\n");
}

