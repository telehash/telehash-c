#include <stddef.h>

#ifndef _b32_h_
#define _b32_h_

size_t base32_encode_length(size_t rawLength);
size_t base32_decode_length(size_t base32Length);
void base32_encode_into(const void *_buffer, size_t bufLen, char *base32Buffer);
char *base32_encode(const void *buf, size_t len);
size_t base32_decode_into(const char *base32Buffer, size_t base32BufLen, void *_buffer);
void *base32_decode(const char *buf, size_t *outlen);
#endif

