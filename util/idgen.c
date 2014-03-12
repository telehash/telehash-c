#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "crypt.h"
#include "packet.h"

#include "platform.h"

int main(int argc, char *argv[])
{
  packet_t keys;
  FILE *fdout;

  if(argc > 2)
  {
    printf("Usage: idgen [idfile.json]\n");
    return -1;
  }
  crypt_init();
  keys = packet_new();
#ifdef CS_1a
#ifdef DEBUG
  printf("*** Generating CS_1a keys ***\n");
#endif
  crypt_keygen(0x1a,keys);
#elif CS_2a
  crypt_keygen(0x2a,keys);
#elif CS_3a
  crypt_keygen(0x3a,keys);
#endif

#ifdef DEBUG
  printf("*** keys=%s ***\n",keys->json);
  printf("*** length=%d ***\n",keys->json_len);
#endif 
  if(argc==1) {
    fdout = fopen("id.json","wb");
    printf("Writing keys to id.json\n");
  } else {
    fdout = fopen(argv[1],"wb");
    printf("Writing keys to %s\n", argv[1]);
  }
  fwrite(keys->json,1,keys->json_len,fdout);
  fclose(fdout);
  return 0;
}
