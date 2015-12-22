#ifndef hashname_h
#define hashname_h

#include "base32.h"
#include "lob.h"

// overall type
typedef struct hashname_struct *hashname_t;

// bin must be 32 bytes if given
hashname_t hashname_new(uint8_t *bin);
hashname_t hashname_free(hashname_t hn);

// these all create a new hashname_t
hashname_t hashname_str(const char *str); // from a string
hashname_t hashname_keys(lob_t keys);
hashname_t hashname_key(lob_t key, uint8_t id); // key is body, intermediates in json

// accessors
uint8_t *hashname_bin(hashname_t hn); // 32 bytes
char *hashname_char(hashname_t hn); // 52 byte base32 string w/ \0 (TEMPORARY)
char *hashname_short(hashname_t hn); // 16 byte base32 string w/ \0 (TEMPORARY)

// utilities related to hashnames
uint8_t hashname_valid(const char *str); // validate a str is a hashname
uint8_t hashname_id(lob_t a, lob_t b); // best matching id (single byte)
lob_t hashname_im(lob_t keys, uint8_t id); // intermediate hashes in the json, optional id to set that as body


#endif
