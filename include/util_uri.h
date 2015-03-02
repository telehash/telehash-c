#ifndef util_uri_h
#define util_uri_h

#include <stdint.h>
#include "lob.h"

// utils to parse and generate a telehash URI

typedef struct util_uri_struct
{
  char *protocol;
  char *hostname;
  uint32_t port;
  char *path;
  lob_t query;
  char *fragment;

  char *encoded;
} *util_uri_t;

// decodes a string into a uri, validates/defaults protocol:// if given, ensures ->address exists
util_uri_t util_uri_new(char *encoded, char *protocol);
util_uri_t util_uri_free(util_uri_t uri);

// produces string safe to use until next encode or free
char *util_uri_encode(util_uri_t uri);

// setters
util_uri_t util_uri_protocol(util_uri_t uri, char *protocol);
util_uri_t util_uri_hostname(util_uri_t uri, char *hostname);
util_uri_t util_uri_port(util_uri_t uri, uint32_t port);
util_uri_t util_uri_path(util_uri_t uri, char *path);
util_uri_t util_uri_keys(util_uri_t uri, lob_t keys);
util_uri_t util_uri_fragment(util_uri_t uri, char *token);

#endif
