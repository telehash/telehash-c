#include <tomcrypt.h>

int main(int argc, char *argv[])
{
  prng_state prng;
  int err;
  unsigned char out[2048], b64[2048];
  unsigned long len, len2;
  rsa_key rkey;
  FILE *fdout;

  ltc_mp = ltm_desc;
  register_prng(&yarrow_desc);

  if(argc < 2)
  {
    printf("./idgen idfile.json\n");
    return -1;
  }
  fdout = fopen(argv[1],"wb");
  fprintf(fdout,"{\"public\":\"");

  // make the key
  if ((err = rng_make_prng(128, find_prng("yarrow"), &prng, NULL)) != CRYPT_OK) {
    printf("Error setting up PRNG, %s\n", error_to_string(err)); return -1;
  }

  if ((err = rsa_make_key(&prng, find_prng("yarrow"), 256, 65537, &rkey)) != CRYPT_OK) {
    printf("rsa_make_key error: %s\n", error_to_string(err));
    return -1;
  }

  // public key  
  len = sizeof(out);
  if ((err = rsa_export(out, &len, PK_PUBLIC, &rkey)) != CRYPT_OK) {
    printf("Export error: %s\n", error_to_string(err));
    return -1;
  }
  printf("rsa key size %d\n",(int)len);
  
  len2 = sizeof(b64);
  if ((err = base64_encode(out, len, b64, &len2)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  fwrite(b64,1,len2,fdout);
  fprintf(fdout,"\",\"private\":\"");

  // private key
  len = sizeof(out);
  if ((err = rsa_export(out, &len, PK_PRIVATE, &rkey)) != CRYPT_OK) {
    printf("Export error: %s\n", error_to_string(err));
    return -1;
  }
  printf("rsa private key size %d\n",(int)len);
  
  len2 = sizeof(b64);
  if ((err = base64_encode(out, len, b64, &len2)) != CRYPT_OK) {
    printf("base64 error: %s\n", error_to_string(err));
    return -1;
  }
  fwrite(b64,1,len2,fdout);
  fprintf(fdout,"\"}");

  fclose(fdout);
  return 0;
}
