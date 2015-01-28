/* some very basic public-domain base64 functions */
#include "base64.h"
#include <stdint.h>
#include <string.h>

// decode str of len into out (must be base64_decode_length(len) bit), return actual decoded len
size_t base64_decode(const char *str, size_t len, uint8_t *out)
{
    const char *cur;
    uint8_t *start;
    int d, dlast, phase;
    char c;
    static char table[256] = {
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
        if(d != -1)
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
size_t base64_encode(const uint8_t* data, size_t input_length, char* encoded_data) {
  static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                  'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                  '4', '5', '6', '7', '8', '9', '-', '_'
                                 };
  static int mod_table[] = {0, 2, 1};

    uint16_t output_length = 4 * ((input_length + 2) / 3);

    for (size_t i = 0, j = 0; i < input_length;) {
      uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
      uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
      uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

      uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

      encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
      encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
      encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
      encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    uint16_t final_length = output_length;
    for (int i = 0; i < mod_table[input_length % 3]; i++) {
      final_length--;
      encoded_data[output_length - 1 - i] = '=';
    }
    encoded_data[output_length] = '\0';
    
    // return len w/o padding
    return final_length;
  }
