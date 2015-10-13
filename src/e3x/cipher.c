#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "telehash.h"
#include "telehash.h"

e3x_cipher_t e3x_cipher_sets[CS_MAX];
e3x_cipher_t e3x_cipher_default = NULL;

uint8_t e3x_cipher_init(lob_t options)
{
  e3x_cipher_default = NULL;
  memset(e3x_cipher_sets, 0, CS_MAX * sizeof(e3x_cipher_t));
  
  e3x_cipher_sets[CS_1a] = cs1a_init(options);
  if(e3x_cipher_sets[CS_1a]) e3x_cipher_default = e3x_cipher_sets[CS_1a];
  if(lob_get(options, "err")) return 1;
  
  if(lob_get_cmp(options, "force", "1a") == 0) return 0;

  e3x_cipher_sets[CS_2a] = cs2a_init(options);
  if(e3x_cipher_sets[CS_2a]) e3x_cipher_default = e3x_cipher_sets[CS_2a];
  if(lob_get(options, "err")) return 1;

  e3x_cipher_sets[CS_3a] = cs3a_init(options);
  if(e3x_cipher_sets[CS_3a]) e3x_cipher_default = e3x_cipher_sets[CS_3a];
  if(lob_get(options, "err")) return 1;

  return 0;
}

e3x_cipher_t e3x_cipher_set(uint8_t csid, char *str)
{
  uint8_t i;
  
  if(!csid && str && strlen(str) == 2) util_unhex(str,2,&csid);

  for(i=0; i<CS_MAX; i++)
  {
    if(!e3x_cipher_sets[i]) continue;
    if(e3x_cipher_sets[i]->csid == csid) return e3x_cipher_sets[i];
    // if they list alg's they support, match on that too
    if(str && e3x_cipher_sets[i]->alg && strstr(e3x_cipher_sets[i]->alg,str)) return e3x_cipher_sets[i];
  }

  return NULL;
}

