#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "util.h"


unsigned char *util_hex(unsigned char *in, int len, unsigned char *out)
{
    int j;
    unsigned char *c = out;

    for (j = 0; j < len; j++) {
      sprintf((char*)c,"%02x", in[j]);
      c += 2;
    }
    *c = '\0';
    return out;
}

unsigned char hexcode(unsigned char x)
{
    if (x >= '0' && x <= '9')         /* 0-9 is offset by hex 30 */
      return (x - 0x30);
    else if (x >= 'A' && x <= 'F')    /* A-F offset by hex 37 */
      return(x - 0x37);
    else if (x >= 'a' && x <= 'f')    /* a-f offset by hex 37 */
      return(x - 0x57);
    else {                            /* Otherwise, an illegal hex digit */
		return x;
    }
}

unsigned char *util_unhex(unsigned char *in, int len, unsigned char *out)
{
	int j;
	unsigned char *c = out;
	for(j=0; (j+1)<len; j+=2)
	{
		*c = ((hexcode(in[j]) * 16) & 0xF0) + (hexcode(in[j+1]) & 0xF);
		c++;
	}
	return out;
}

int util_cmp(char *a, char *b)
{
  if(!a || !b) return -1;
  if(a == b) return 0;
  return strcasecmp(a,b);
}

static inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

uint32_t util_mmh32(const uint8_t * data, int len)
{
    const int nblocks = len / 4;

    uint32_t h1 = 0;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t * blocks = (const uint32_t*) (data + nblocks * 4);

    int i;
    for(i = -nblocks; i; i++)
    {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = rotl32(h1, 13);
        h1 = h1*5+0xe6546b64;
    }

    //----------
    // tail

    const uint8_t * tail = (const uint8_t*)(data + nblocks*4);

    uint32_t k1 = 0;

    switch(len & 3)
    {
        case 3: k1 ^= tail[2] << 16;
        case 2: k1 ^= tail[1] << 8;
        case 1: k1 ^= tail[0];
              k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
    }

    //----------
    // finalization

    h1 ^= len;

    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

char *util_murmur(const unsigned char* data, int len, char *hex)
{
  uint32_t hash = util_mmh32((uint8_t*)data,len);
  sprintf(hex,"%08lx",(unsigned long)hash);
  return hex;
}