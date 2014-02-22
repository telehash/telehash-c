#include "crypt.h"
#include "packet.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc, char *argv[])
{
  packet_t keys;
  FILE *fdout;

  if(argc < 2)
  {
    printf("./idgen idfile.json\n");
    return -1;
  }
  crypt_init();
  keys = packet_new();
  crypt_keygen(0x2a,keys);
  fdout = fopen(argv[1],"wb");
  fwrite(keys->json,1,keys->json_len,fdout);
  fclose(fdout);
  return 0;
}
