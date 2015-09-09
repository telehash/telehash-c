/* some very basic public-domain base64 functions */
#include "telehash.h"
#include <stdint.h>
#include <string.h>

// decode str of len into out (must be base64_decode_length(len) bit), return actual decoded len
size_t base64_decoder(const char *str, size_t len, uint8_t *out)
{
    const char *cur;
    uint8_t *start;
    int8_t d;
    uint8_t c, phase, dlast;
    static int8_t table[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,62,-1,63,  /* 20-2F */
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,63,  /* 50-5F */
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
    };

    if(!str || !out) return 0;
    if(!len) len = strlen(str);

    d = dlast = phase = 0;
    start = out;
    for (cur = str; *cur != '\0' && len; ++cur, --len)
    {
        /* handle newlines as seperate chunks */
        if(*cur == '\n' || *cur == '\r')
        {
            phase = dlast = 0;
            continue;
        }

        d = table[(int)*cur];
        if(d >= 0)
        {
            switch(phase)
            {
            case 0:
                ++phase;
                break;
            case 1:
                c = ((dlast << 2) | ((d & 0x30) >> 4));
                *out++ = c;
                ++phase;
                break;
            case 2:
                c = (((dlast & 0xf) << 4) | ((d & 0x3c) >> 2));
                *out++ = c;
                ++phase;
                break;
            case 3:
                c = (((dlast & 0x03 ) << 6) | d);
                *out++ = c;
                phase = 0;
                break;
            }
            dlast = d;
        }
    }
    *out = '\0';
    return out - start;
}



// encode str of len into out (must be at least base64_encode_length(len) big), return encoded len
size_t base64_encoder(const uint8_t *str, size_t len, char *out)
{
    size_t i;
    uint8_t s1, s2;
    char *cur = out;
    static uint8_t table[64] = {
        'A','B','C','D','E','F','G','H',
        'I','J','K','L','M','N','O','P',
        'Q','R','S','T','U','V','W','X',
        'Y','Z','a','b','c','d','e','f',
        'g','h','i','j','k','l','m','n',
        'o','p','q','r','s','t','u','v',
        'w','x','y','z','0','1','2','3',
        '4','5','6','7','8','9','-','_'
    };

    if(!str || !out || !len) return 0;

    for (i = 0; i < len; i += 3, str += 3)
    {
      s1 = (i+1<len)?str[1]:0;
      s2 = (i+2<len)?str[2]:0;
      *cur++ = table[str[0] >> 2];
      *cur++ = table[((str[0] & 3) << 4) + (s1 >> 4)];
      *cur++ = table[((s1 & 0xf) << 2) + (s2 >> 6)];
      *cur++ = table[s2 & 0x3f];
    }

    if (i == len + 1)
        *(cur - 1) = '=';
    else if (i == len + 2)
        *(cur - 1) = *(cur - 2) = '=';
    *cur = '\0';

    // return actual length, not padded
    return (cur - out) - (i-len);
}
