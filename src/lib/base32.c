/* (PD) 2001 The Bitzi Corporation
 * Please see file COPYING or http://bitzi.com/publicdomain 
 * for more info.
 *
 * $Id: base32.c,v 1.5 2006/03/18 16:28:50 mhe Exp $
 *
 * Modified by Martin Hedenfalk 2005 for use in ShakesPeer.
 */

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define BASE32_LOOKUP_MAX 43
static char *base32Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static unsigned char base32Lookup[BASE32_LOOKUP_MAX][2] =
{
    { '0', 0xFF },
    { '1', 0xFF },
    { '2', 0x1A },
    { '3', 0x1B },
    { '4', 0x1C },
    { '5', 0x1D },
    { '6', 0x1E },
    { '7', 0x1F },
    { '8', 0xFF },
    { '9', 0xFF },
    { ':', 0xFF },
    { ';', 0xFF },
    { '<', 0xFF },
    { '=', 0xFF },
    { '>', 0xFF },
    { '?', 0xFF },
    { '@', 0xFF },
    { 'A', 0x00 },
    { 'B', 0x01 },
    { 'C', 0x02 },
    { 'D', 0x03 },
    { 'E', 0x04 },
    { 'F', 0x05 },
    { 'G', 0x06 },
    { 'H', 0x07 },
    { 'I', 0x08 },
    { 'J', 0x09 },
    { 'K', 0x0A },
    { 'L', 0x0B },
    { 'M', 0x0C },
    { 'N', 0x0D },
    { 'O', 0x0E },
    { 'P', 0x0F },
    { 'Q', 0x10 },
    { 'R', 0x11 },
    { 'S', 0x12 },
    { 'T', 0x13 },
    { 'U', 0x14 },
    { 'V', 0x15 },
    { 'W', 0x16 },
    { 'X', 0x17 },
    { 'Y', 0x18 },
    { 'Z', 0x19 }
};

int base32_encode_length(int rawLength)
{
    return ((rawLength * 8) / 5) + ((rawLength % 5) != 0) + 1;
}

int base32_decode_length(int base32Length)
{
    return ((base32Length * 5) / 8);
}

void base32_encode_into(const void *_buffer, unsigned int bufLen, char *base32Buffer)
{
    unsigned int i, index;
    unsigned char word;
    const unsigned char *buffer = _buffer;

    for(i = 0, index = 0; i < bufLen;)
    {
        /* Is the current word going to span a byte boundary? */
        if (index > 3)
        {
            word = (buffer[i] & (0xFF >> index));
            index = (index + 5) % 8;
            word <<= index;
            if (i < bufLen - 1)
                word |= buffer[i + 1] >> (8 - index);

            i++;
        }
        else
        {
            word = (buffer[i] >> (8 - (index + 5))) & 0x1F;
            index = (index + 5) % 8;
            if (index == 0)
                i++;
        }

        assert(word < 32);
        *(base32Buffer++) = (char)base32Chars[word];
    }

    *base32Buffer = 0;
}

char *base32_encode(const void *buf, unsigned int len)
{
    char *tmp = malloc(base32_encode_length(len));
    base32_encode_into(buf, len, tmp);
    return tmp;
}

int base32_decode_into(const char *base32Buffer, unsigned int base32BufLen, void *_buffer)
{
    int i, index, max, lookup, offset;
    unsigned char  word;
    unsigned char *buffer = _buffer;

    memset(buffer, 0, base32_decode_length(base32BufLen));
    max = strlen(base32Buffer);
    for(i = 0, index = 0, offset = 0; i < max; i++)
    {
        lookup = toupper(base32Buffer[i]) - '0';
        /* Check to make sure that the given word falls inside
           a valid range */
        if ( lookup < 0 && lookup >= BASE32_LOOKUP_MAX)
            word = 0xFF;
        else
            word = base32Lookup[lookup][1];

        /* If this word is not in the table, ignore it */
        if (word == 0xFF)
            continue;

        if (index <= 3)
        {
            index = (index + 5) % 8;
            if (index == 0)
            {
                buffer[offset] |= word;
                offset++;
            }
            else
                buffer[offset] |= word << (8 - index);
        }
        else
        {
            index = (index + 5) % 8;
            buffer[offset] |= (word >> index);
            offset++;

            buffer[offset] |= word << (8 - index);
        }
    }
    return offset;
}

void *base32_decode(const char *buf, unsigned int *outlen)
{
    unsigned int len = strlen(buf);
    char *tmp = malloc(base32_decode_length(len));
    unsigned int x = base32_decode_into(buf, len, tmp);
    if(outlen)
        *outlen = x;
    return tmp;
}

