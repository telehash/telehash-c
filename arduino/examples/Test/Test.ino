#include <AESLib.h>
#include <telehash.h>

#define sp Serial.print
#define speol Serial.println

/* scratch
aes AC 05 4E B1 17 79 
sha1 6E CF F8 78 5A 7A 1D 2B 88 B1 C1 23 03 E9 E2 19 0A A6 4F 89 
hmac 3C 83 F4 E4 3A 37 CE 3E 24 32 39 B7 CF 37 52 96 99 3D 11 0C 
sha256 FF FF E3 8F FF FF EA 28 00 00 12 FB 00 00 63 AA FF FF D2 3A 00 00 5B 8E FF FF 9A D6 00 00 43 B7 
Testing random private key pairs
473 gen
475 gen
472 dh
476 dh

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
  char buf[256];
  sp(util_hex(p_vli,p_size,buf));
}


void setup() {
  Serial.begin(115200);
}

int uECC_test(int loops)
{
    int i;
    
    uint8_t l_private1[uECC_BYTES];
    uint8_t l_private2[uECC_BYTES];
    
    uint8_t l_public1[uECC_BYTES *2];
    uint8_t l_public2[uECC_BYTES *2];
    
    uint8_t l_secret1[uECC_BYTES];
    uint8_t l_secret2[uECC_BYTES];

    uECC_set_rng(&RNG);
    
    speol("Testing random private key pairs");

    for(i=0; i<loops; ++i)
    {
        long a = millis();
        uECC_make_key(l_public1, l_private1);
        sp(millis()-a);
        speol(" gen");
        a = millis();
        uECC_make_key(l_public2, l_private2);
        sp(millis()-a);
        speol(" gen");


        a = millis();
        if(!uECC_shared_secret(l_public2, l_private1, l_secret1))
        {
            sp("shared_secret() failed (1)\n");
            return 1;
        }
        sp(millis()-a);
        speol(" dh");

        a = millis();
        if(!uECC_shared_secret(l_public1, l_private2, l_secret2))
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
            vli_print(l_secret1, uECC_BYTES);
            sp("\n");
            sp("Shared secret 2 = ");
            vli_print(l_secret2, uECC_BYTES);
            sp("\n");
            sp("Private key 1 = ");
            vli_print(l_private1, uECC_BYTES);
            sp("\n");
            sp("Private key 2 = ");
            vli_print(l_private2, uECC_BYTES);
            sp("\n");
        }
    }
    sp("\n");
    
    return 0;
}

int aes_test()
{
  unsigned char key[16], data[6],iv[16],block[16];
  size_t off = 0;
  memcpy(key,"1234567812345678",16);
  memset(iv,0,16);
  memcpy(data,"foobar",6);
  aes_128_ctr(key,6,iv,data,data);
  sp("aes ");
  vli_print(data,6);
  speol();
}

int sha256_test()
{
  unsigned char hash[SHA256_HASH_BYTES];
  sha256((uint8_t (*)[32])hash,"foo",3*8);
  sp("sha256 ");
  vli_print(hash,SHA256_HASH_BYTES);
  speol();
}

int hmac_test()
{
  unsigned char hmac[32];
  hmac_256((unsigned char*)"foo",3,(unsigned char*)"bar",3,hmac);
  sp("hmac ");
  vli_print(hmac,32);
  speol();
}

int keygen()
{
//  packet_t keys = packet_new();
//  crypt_keygen(0x1a,keys);
//  Serial.write(keys->json,keys->json_len);
  speol();
}

void loop() {
  long start = millis();
  Serial.write("hi\n");
  keygen();
  aes_test();
  hmac_test();
  sha256_test();
  sp(millis() - start);
  uECC_test(5);
  speol();
}

