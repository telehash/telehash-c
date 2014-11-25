#ifndef uri_h
#define uri_h

#include <stdint.h>
#include "lob.h"

// utils to parse and generate a telehash URI

typedef struct uri_struct
{
  char *protocol;
  char *user;
  char *canonical;
  char *address;
  uint32_t port;
  char *session;
  lob_t keys;
  char *token;

  char *encoded;
} *uri_t;

// decodes a string into a uri, validates/defaults protocol:// if given, ensures ->address exists
uri_t uri_new(char *encoded, char *protocol);
uri_t uri_free(uri_t uri);

// produces string safe to use until next encode or free
char *uri_encode(uri_t uri);

// setters
uri_t uri_protocol(uri_t uri, char *protocol);
uri_t uri_user(uri_t uri, char *user);
uri_t uri_canonical(uri_t uri, char *canonical);
uri_t uri_address(uri_t uri, char *address);
uri_t uri_port(uri_t uri, uint32_t port);
uri_t uri_session(uri_t uri, char *session);
uri_t uri_keys(uri_t uri, lob_t keys);
uri_t uri_token(uri_t uri, char *token);

#endif