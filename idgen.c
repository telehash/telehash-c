#include <tomcrypt.h>

int main(int argc, char *argv[])
{
  prng_state prng;
  int err, cursor;
  unsigned char out[1024];
  unsigned long len;
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
  cursor = fprintf(fdout,"{\"public\":\"");

  if ((err = rng_make_prng(128, find_prng("yarrow"), &prng, NULL)) != CRYPT_OK) {
    printf("Error setting up PRNG, %s\n", error_to_string(err)); return -1;
  }

  if ((err = rsa_make_key(&prng, find_prng("yarrow"), 256, 65537, &rkey)) != CRYPT_OK) {
    printf("rsa_make_key error: %s\n", error_to_string(err));
    return -1;
  }

  len = sizeof(out);
  if ((err = rsa_export(out, &len, PK_PUBLIC, &rkey)) != CRYPT_OK) {
    printf("Export error: %s\n", error_to_string(err));
    return -1;
  }
  printf("rsa key size %d\n",(int)len);

  fclose(fdout);
  return 0;
}
