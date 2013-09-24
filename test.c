#include <tomcrypt.h>
#include <tommath.h>

// http://forum.doom9.org/showthread.php?t=157470

char *hex(unsigned char *digest, int len, char *out);

int main(void)
{
  ecc_key mykey, urkey;
  prng_state prng;
  int err, x;
  unsigned char out[4096], str[1024], iv[16], key[32];
  unsigned char *foo = "just testing";
  unsigned long len;
  symmetric_CTR ctr;
  rsa_key rkey;

  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);

  if ((err = rng_make_prng(128, find_prng("yarrow"), &prng, NULL)) != CRYPT_OK) {
    printf("Error setting up PRNG, %s\n", error_to_string(err)); return -1;
  }

  // ECC TESTING
  if ((err = ecc_make_key(&prng, find_prng("yarrow"), 32, &mykey)) != CRYPT_OK) {
    printf("Error making key: %s\n", error_to_string(err)); return -1;
  }
  
  mp_to_unsigned_bin(mykey.pubkey.x, str);
  x = mp_unsigned_bin_size(mykey.pubkey.y);  
  printf("ecc key x %d X %s\n",x,hex(str,x,out));

  if ((err = ecc_make_key(&prng, find_prng("yarrow"), 32, &urkey)) != CRYPT_OK) {
    printf("Error making key: %s\n", error_to_string(err)); return -1;
  }

  x = sizeof(str);
  if ((err = ecc_shared_secret(&mykey,&urkey,str,&x)) != CRYPT_OK) {
    printf("Error making key: %s\n", error_to_string(err)); return -1;
  }
  printf("shared secret %d %s\n",x,hex(str,x,out));
  


  // SHA256 TESTING
  len = 32;
  if ((err = hash_memory(find_hash("sha256"),foo,strlen(foo),key,&len)) != CRYPT_OK) {
     printf("Error hashing key: %s\n", error_to_string(err));
     return -1;
  }
  printf("sha256 %s\n",hex(key,32,out));


  // AES TESTING
  x = yarrow_read(iv,16,&prng);
  if (x != 16) {
    printf("Error reading PRNG for IV required.\n");
    return -1;
  }
  printf("random IV %s\n",hex(iv,16,out));

  if ((err = ctr_start(find_cipher("aes"),iv,key,32,0,CTR_COUNTER_LITTLE_ENDIAN,&ctr)) != CRYPT_OK) {
     printf("ctr_start error: %s\n",error_to_string(err));
     return -1;
  }

  if ((err = ctr_encrypt(foo,str,strlen(foo),&ctr)) != CRYPT_OK) {
     printf("ctr_encrypt error: %s\n", error_to_string(err));
     return -1;
  }
  printf("aes encrypt %s\n",hex(str,strlen(foo),out));


  // RSA TESTING
  if ((err = rsa_make_key(&prng, find_prng("yarrow"), 256, 65537, &rkey)) != CRYPT_OK) {
    printf("rsa_make_key error: %s\n", error_to_string(err));
    return -1;
  }

  x = sizeof(str);
  if ((err = rsa_export(str, &x, PK_PUBLIC, &rkey)) != CRYPT_OK) {
    printf("Export error: %s\n", error_to_string(err));
    return -1;
  }
  printf("rsa key size %d key %s\n",x,hex(str,x,out));

  x = sizeof(str);
  if ((err = rsa_encrypt_key(foo, strlen(foo), str, &x, "TestApp", 7, &prng, find_prng("yarrow"), find_hash("sha1"), &rkey)) != CRYPT_OK) {
    printf("rsa_encrypt error: %s\n", error_to_string(err));
    return -1;
  }
  printf("rsa enc size %d of %s\n",x,hex(str,x,out));


  return 0;
}

char *hex(unsigned char *digest, int len, char *out)
{
    int i,j;
    char *c = out;

    for (i = 0; i < len/4; i++) {
        for (j = 0; j < 4; j++) {
            sprintf(c,"%02x", digest[i*4+j]);
            c += 2;
        }
        sprintf(c, " ");
        c += 1;
    }
    *(c - 1) = '\0';
    return out;
}

