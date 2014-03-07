#include <SPI.h>
#include <Wire.h>
#include <Scout.h>
#include <GS.h>
#include <bitlash.h>
#include <lwm.h>
#include <js0n.h>

extern "C" {
#include "ecc.h"
#include "./aes.h"
#include "./sha256.h"
#include "./sha1.h"
#include "./hmac.h"
}

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
      sp(hex);
      --p_size;
    }
}


void setup() {
  Scout.setup();
}

int etest(int loops)
{
    int i;
    
    uint8_t l_private1[ECC_BYTES];
    uint8_t l_private2[ECC_BYTES];
    
    uint8_t l_public1[ECC_BYTES *2];
    uint8_t l_public2[ECC_BYTES *2];
    
    uint8_t l_secret1[ECC_BYTES];
    uint8_t l_secret2[ECC_BYTES];

    ecc_set_rng(&RNG);
    
    speol("Testing random private key pairs");

    for(i=0; i<loops; ++i)
    {
        long a = millis();
        ecc_make_key(l_public1, l_private1);
        sp(millis()-a);
        speol(" gen");
        a = millis();
        ecc_make_key(l_public2, l_private2);
        sp(millis()-a);
        speol(" gen");


        a = millis();
        if(!ecdh_shared_secret(l_public2, l_private1, l_secret1))
        {
            sp("shared_secret() failed (1)\n");
            return 1;
        }
        sp(millis()-a);
        speol(" dh");

        a = millis();
        if(!ecdh_shared_secret(l_public1, l_private2, l_secret2))
        {
            sp("shared_secret() failed (2)\n");
            return 1;
        }
        sp(millis()-a);
        speol(" dh");
        
        if(memcmp(l_secret1, l_secret2, sizeof(l_secret1)) != 0)
        {
            sp("Shared secrets are not identical!\n");
            sp("Shared secret 1 = ");
            vli_print(l_secret1, ECC_BYTES);
            sp("\n");
            sp("Shared secret 2 = ");
            vli_print(l_secret2, ECC_BYTES);
            sp("\n");
            sp("Private key 1 = ");
            vli_print(l_private1, ECC_BYTES);
            sp("\n");
            sp("Private key 2 = ");
            vli_print(l_private2, ECC_BYTES);
            sp("\n");
        }
    }
    sp("\n");
    
    return 0;
}

int atest()
{
  aes_context ctx;
  char key[16];
  aes_setkey_enc(&ctx,(unsigned char*)key,16);
}

int s2test()
{
  sha256_ctx_t ctx;
  sha256_hash_t h;
  sha256_init(&ctx);
}

int s1test()
{
  hmac_sha1_ctx_t ctx;
  hmac_sha1_init(&ctx,"foo",3);
}

void loop() {
  Scout.loop();
  long start = millis();
  atest();
  s1test();
  s2test();
  sp(millis() - start);
  etest(5);
  speol();
}

