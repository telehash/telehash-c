// depends on AESLib from https://github.com/DavyLandman/AESLib, add to your libraries and #include <AESLib.h> in the sketch
#include "aes.h"
#include "bcal-basic.h"
#include "bcal_aes128.h"

void aes_128_ctr(unsigned char *key, size_t length, unsigned char nonce_counter[16], const unsigned char *input, unsigned char *output)
{
    int c, i;
    size_t n = 0;
    unsigned char stream_block[16];
    bcgen_ctx_t cctx;
    bcal_cipher_init(&aes128_desc, key, 128, &cctx);

    while( length-- )
    {
        if( n == 0 ) {
          memcpy(stream_block,nonce_counter,16);
          bcal_cipher_enc(stream_block, &cctx);

            for( i = 16; i > 0; i-- )
                if( ++nonce_counter[i - 1] != 0 )
                    break;
        }
        c = *input++;
        *output++ = (unsigned char)( c ^ stream_block[n] );

        n = (n + 1) & 0x0F;
    }
  }

