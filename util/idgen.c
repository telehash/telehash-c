#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>

#include "e3x.h"
#include "lob.h"

#include "platform.h"

int main(int argc, char *argv[])
{
  lob_t keys, options;
  FILE *fdout;

  if(argc > 2)
  {
    printf("Usage: idgen [idfile.json]\n");
    return -1;
  }

  options = lob_new();
  if(!e3x_init(options))
  {
    printf("e3x init failed: %s\n",lob_get(options,"err"));
    return -1;
  }

  DEBUG_PRINTF("*** generating keys ***");
  keys = e3x_generate();
  if(!keys)
  {
    printf("keygen failed: %s\n",e3x_err());
    return -1;
  }

  if(argc==1) {
    fdout = fopen("id.json","wb");
    printf("Writing keys to id.json\n");
  } else {
    fdout = fopen(argv[1],"wb");
    printf("Writing keys to %s\n", argv[1]);
  }
  if(fdout == NULL) {
	printf("Error opening file\n");
	return 1;
  }
  fwrite(keys->head,1,keys->head_len,fdout);
  fclose(fdout);
  return 0;
}
