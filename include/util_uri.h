#ifndef util_uri_h
#define util_uri_h

#include <stdint.h>
#include "lob.h"

// utils to get link info in/out of a uri

// parser
lob_t util_uri_parse(char *string);

// get keys from query
lob_t util_uri_keys(lob_t uri);

// get paths from host and query
lob_t util_uri_paths(lob_t uri); // chain

// validate any fragment from this peer
uint8_t util_uri_check(lob_t uri, uint8_t *peer);

// generators
lob_t util_uri_add_keys(lob_t uri, lob_t keys);
lob_t util_uri_add_path(lob_t uri, lob_t path);
lob_t util_uri_add_check(lob_t uri, uint8_t *peer, uint8_t *data, size_t len);
lob_t util_uri_add_data(lob_t uri, uint8_t *data, size_t len);

// serialize out from lob format to "uri" key and return it
char *util_uri_format(lob_t uri);

#endif
