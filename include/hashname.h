#ifndef hashname_h
#define hashname_h

#include "base32.h"
#include "lob.h"

// overall type
typedef struct hashname_struct *hashname_t;

// only things that actually malloc/free
hashname_t hashname_dup(hashname_t hn);
hashname_t hashname_free(hashname_t hn);

// everything else returns a pointer to a static global for temporary use
hashname_t hashname_vchar(const char *str); // from a string
hashname_t hashname_vbin(const uint8_t *bin);
hashname_t hashname_vkeys(lob_t keys);
hashname_t hashname_vkey(lob_t key, uint8_t id); // key is body, intermediates in json

// accessors
uint8_t *hashname_bin(hashname_t hn); // 32 bytes
char *hashname_char(hashname_t hn); // 52 byte base32 string w/ \0 (TEMPORARY)

// utilities related to hashnames
int hashname_cmp(hashname_t a, hashname_t b);  // memcmp shortcut
uint8_t hashname_id(lob_t a, lob_t b); // best matching id (single byte)
lob_t hashname_im(lob_t keys, uint8_t id); // intermediate hashes in the json, optional id to set that as body

// working with short hashnames (5 bin bytes, 8 char bytes)
char *hashname_short(hashname_t hn); // 8 byte base32 string w/ \0 (TEMPORARY)
int hashname_scmp(hashname_t a, hashname_t b);  // short only comparison
hashname_t hashname_schar(const char *str); // 8 char string, temp hn
hashname_t hashname_sbin(const uint8_t *bin); // 5 bytes, temp hn
hashname_t hashname_isshort(hashname_t hn); // NULL unless is short

#endif
