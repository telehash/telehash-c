// depends on AESLib from https://github.com/DavyLandman/AESLib, add to your libraries and #include <AESLib.h> in the sketch
#include "aes.h"
#include "aes_keyschedule.h"

void aes_128_ctr(unsigned char *key, size_t length, unsigned char nonce_counter[16], const unsigned char *input, unsigned char *output)
{
    int c, i;
    size_t n = 0;
    unsigned char stream_block[16];
    aes128_ctx_t ctx;
    aes128_init(key, &ctx);

    while( length-- )
    {
        if( n == 0 ) {
          memcpy(stream_block,nonce_counter,16);
          aes128_enc(stream_block, &ctx);

            for( i = 16; i > 0; i-- )
                if( ++nonce_counter[i - 1] != 0 )
                    break;
        }
        c = *input++;
        *output++ = (unsigned char)( c ^ stream_block[n] );

        n = (n + 1) & 0x0F;
    }
}

