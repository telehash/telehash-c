#ifndef util_h
#define util_h

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util_sys.h"
#include "util_uri.h"
#include "util_chunks.h"
#include "util_frames.h"
#include "util_unix.h"

// make sure out is 2*len + 1
char *util_hex(uint8_t *in, size_t len, char *out);
// out must be len/2
uint8_t *util_unhex(char *in, size_t len, uint8_t *out);
// hex string validator, NULL is invalid, else returns str
char *util_ishex(char *str, uint32_t len);

// safer string comparison (0 == same)
int util_cmp(char *a, char *b);

// portable sort
void util_sort(void *base, unsigned int nel, unsigned int width, int (*comp)(void *, const void *, const void *), void *arg);

// portable reallocf
void *util_reallocf(void *ptr, size_t size);

// get a "now" timestamp to do millisecond timers
uint64_t util_at(void); // only pass at into _since()
uint32_t util_since(uint64_t at); // get ms since the at

// Use a constant time comparison function to avoid timing attacks
int util_ct_memcmp(const void* s1, const void* s2, size_t n);

// embedded may not have strdup but it's a kinda handy shortcut
char *util_strdup(const char *str);
#ifndef strdup
#define strdup util_strdup
#endif

#endif
