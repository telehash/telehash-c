#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "util.h"


static inline uint32_t rotl32(uint32_t x, int8_t r)
{
    return (x << r) | (x >> (32 - r));
}

uint32_t murmur4(const uint32_t *data, uint8_t len)
{
    const int nblocks = len / 4;

    uint32_t h1 = 0;

    uint32_t c1 = 0xcc9e2d51;
    uint32_t c2 = 0x1b873593;

    //----------
    // body

    const uint32_t * blocks = data + nblocks;

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

    const uint8_t * tail = (const uint8_t*)((uint8_t*)data + nblocks*4);

    uint32_t k1 = 0;

    switch(len & 3)
    {
        case 3: k1 ^= (uint32_t) tail[2] << 16;
        case 2: k1 ^= (uint32_t) tail[1] << 8;
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

char *murmur8(const uint32_t* data, uint8_t len, char *hex)
{
  uint32_t hash = murmur4(data,len);
  sprintf(hex,"%08lx",(unsigned long)hash);
  return hex;
}

