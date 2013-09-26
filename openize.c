#include <tomcrypt.h>
#include <tommath.h>
#include <j0g.h>

// http://forum.doom9.org/showthread.php?t=157470

char *hex(unsigned char *digest, int len, char *out);
int hashname(unsigned char *key, int klen, char *hn);

int main(int argc, char *argv[])
{
  ecc_key mykey, urkey;
  prng_state prng;
  int err, x, ilen, seed_port;
  unsigned char buf[4096], out[4096], str[1024], iv[16], key[32], inner[2048], *iat;
  char inbuf[32768], *ibat, *pem, self_hn[64], seed_hn[64], *foo = "just testing", seed_ip[32];
  char *pem_pre = "-----BEGIN PUBLIC KEY-----\n", *pem_suf = "\n-----END PUBLIC KEY-----\n";
  unsigned long len;
  unsigned short index[32];
  symmetric_CTR ctr;
  rsa_key rkey;
  FILE *idfile;

  ltc_mp = ltm_desc;
  register_cipher(&aes_desc);  
  register_prng(&yarrow_desc);
  register_hash(&sha256_desc);
  register_hash(&sha1_desc);

  // first read in the rsa keypair id
  if(argc < 2)
  {
    printf("./openize idfile.json\n");
    return -1;
  }
  idfile = fopen(argv[1],"rb");
  if(!idfile)
  {
    printf("can't open %s\n",argv[1]);
    return -1;
  }
  x = fread(inbuf,1,sizeof(inbuf),idfile);
  fclose(idfile);
  if(x < 100)
  {
    printf("Error reading id json, length %d is too short\n", x);
    return -1;
  }
  inbuf[x] = 0;
  
  // parse the json
  j0g(inbuf, index, 32);
  if(!*index || !j0g_val("public",inbuf,index) || !j0g_val("private",inbuf,index))
  {
    printf("Error parsing json, must be valid and have public and private keys: %s\n",inbuf);
    return -1;
  }
  
  // load the keypair
  len = sizeof(buf);
  if ((err = base64_decode(j0g_str("public",inbuf,index), strlen(j0g_str("public",inbuf,index)), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = rsa_import(buf, len, &rkey)) != CRYPT_OK) {
    printf("Import public error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = hashname(buf, len, self_hn)) != CRYPT_OK) {
    printf("hashname error: %s\n", error_to_string(err));
    return -1;
  }
  len = sizeof(buf);
  if ((err = base64_decode(j0g_str("private",inbuf,index), strlen(j0g_str("private",inbuf,index)), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = rsa_import(buf, len, &rkey)) != CRYPT_OK) {
    printf("Import private error: %s\n", error_to_string(err));
    return -1;
  }  
  printf("loaded keys for %.*s\n",64,self_hn);

  // load the seeds file
  idfile = fopen("seeds.json","rb");
  if(!idfile)
  {
    printf("missing seeds.json\n");
    return -1;
  }
  x = fread(inbuf,1,sizeof(inbuf),idfile);
  fclose(idfile);
  if(x < 100)
  {
    printf("Error reading seeds json, length %d is too short\n", x);
    return -1;
  }
  inbuf[x] = 0;
  
  // parse the json
  j0g(inbuf, index, 32);
  if(!*index || *inbuf != '[' || inbuf[*index] != '{')
  {
    printf("Error parsing seeds json, must be valid and have an array containing an object: %s\n",inbuf);
    return -1;
  }
  inbuf[index[1]+2] = 0; // null terminate the first object
  ibat = inbuf+*index; // now point to just the object
  j0g(ibat, index, 32); // re-parse the inner object
  if(!*index || !j0g_val("pubkey",ibat,index) || !j0g_val("ip",ibat,index) || !j0g_val("port",ibat,index))
  {
    printf("Error parsing seeds json, must contain a valid object having a public, ip, and port: %s\n",ibat);
    return -1;
  }
  
  // load the seed public key and ip:port
  pem = j0g_str("pubkey",ibat,index);
  if(strstr(pem,pem_pre) == pem) pem += strlen(pem_pre);
  if(strstr(pem,pem_suf)) *(strstr(pem,pem_suf)) = 0;
  // pem should now be just the base64
  len = sizeof(buf);
  if ((err = base64_decode(pem, strlen(pem), buf, &len)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = rsa_import(buf, len, &rkey)) != CRYPT_OK) {
    printf("Import public error: %s\n", error_to_string(err));
    return -1;
  }
  if ((err = hashname(buf, len, seed_hn)) != CRYPT_OK) {
    printf("hashname error: %s\n", error_to_string(err));
    return -1;
  }
  strcpy(seed_ip,j0g_str("ip",ibat,index));
  seed_port = atoi(j0g_str("port",ibat,index));
  printf("seed loaded %.*s at %s:%d\n",64,seed_hn,seed_ip,seed_port);
  

  iat = inner+2;
//  sprintf(iat, "{\"line\":\"%s\",\"at\":%d,\"to\":\"%s\"}",);

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
    int j;
    char *c = out;

    for (j = 0; j < len; j++) {
      sprintf(c,"%02x", digest[j]);
      c += 2;
    }
    *c = '\0';
    return out;
}

int hashname(unsigned char *key, int klen, char *hn)
{
  unsigned char bin[32];
  unsigned long len=32;
  int err;
  if ((err = hash_memory(find_hash("sha256"),key,klen,bin,&len)) != CRYPT_OK) return err;
  hex(bin,len,hn);
  return CRYPT_OK;
}