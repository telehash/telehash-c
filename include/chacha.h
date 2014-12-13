#ifndef _CHACHA20_H_
#define _CHACHA20_H_

// public domain, original source: https://gist.github.com/thoughtpolice/2b36e168d2d7582ad58b

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define chacha20_KEYBYTES   32
#define chacha20_NONCEBYTES 8

int chacha20_xor(
  unsigned char*, const unsigned char*,
  unsigned long long,
  const unsigned char*, const unsigned char*);

int chacha20_base(
  unsigned char*, unsigned long long,
  const unsigned char*, const unsigned char*);

// a convert-in-place utility
uint8_t *chacha20(uint8_t *key, uint8_t *nonce, uint8_t *bytes, uint32_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !_CHACHA20_H_ */
