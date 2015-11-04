#ifndef _CHACHA20_H_
#define _CHACHA20_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// a convert-in-place utility
uint8_t *chacha20(uint8_t *key, uint8_t *nonce, uint8_t *bytes, uint32_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !_CHACHA20_H_ */
