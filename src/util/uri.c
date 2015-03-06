#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "util.h"

// parser
lob_t util_uri_parse(char *string)
{
  return NULL;
}

// get keys from query
lob_t util_uri_keys(lob_t uri)
{
  return NULL;
}

// get paths from host and query
lob_t util_uri_paths(lob_t uri)
{
  return NULL;
}

// validate any fragment from this peer
uint8_t util_uri_check(lob_t uri, uint8_t *peer)
{
  return 0;
}

lob_t util_uri_add_keys(lob_t uri, lob_t keys)
{
  return NULL;
}

lob_t util_uri_add_path(lob_t uri, lob_t path)
{
  return NULL;
}

lob_t util_uri_add_check(lob_t uri, uint8_t *peer, uint8_t *data, size_t len)
{
  return NULL;
}

lob_t util_uri_add_data(lob_t uri, uint8_t *data, size_t len)
{
  return NULL;
}


// serialize out from lob format to "uri" key
lob_t util_uri_format(lob_t uri)
{
  return NULL;
}

/*
// this is a very verbose/explicit single-pass telehash uri parser, no magic
util_uri_t util_uri_new(char *encoded, char *protocol)
{
  util_uri_t uri;
  size_t plen, ulen, alen, klen, vlen;
  char *at, *user = NULL, *address, *val;
  char pdef[] = "link";

  if(!encoded) return LOG("bad args");

  // check for protocol:// prefix first
  if(!(at = strstr(encoded,"://")))
  {
    if(!protocol) protocol = (char*)pdef; // default to "link://"
    plen = strlen(protocol);
  }else{
    plen = (size_t)(at - encoded);
    // enforce if specified
    if(protocol && (plen != strlen(protocol) || strncmp(encoded,protocol,plen))) return LOG("protocol mismatch %s != %.*s",protocol,plen,encoded);
    protocol = encoded;
    encoded = at+3;
  }
  
  // check for user@ prefix next
  if((at = strchr(encoded,'@')))
  {
    ulen = (size_t)(at - encoded);
    user = encoded;
    if(!ulen || !islower(user[0])) return LOG("invalid user: '%.*s'",ulen,user);
    // TODO decode optional .base32 alternative
    encoded = at+1;
  }else{
    ulen = 0;
  }
  
  address = encoded;
  alen = strlen(address);

  // ensure there's at least an address
  if(!alen || !isalnum(address[0])) return LOG("invalid address: '%s'",address);

  if(!(uri = malloc(sizeof(struct util_uri_struct)))) return LOG("OOM");
  memset(uri,0,sizeof (struct util_uri_struct));

  uri->protocol = strndup(protocol,plen);
  if(ulen) uri->user = strndup(user,ulen);

  // copy in canonical and parse address/port
  if((at = strchr(encoded,'/')) || (at = strchr(encoded,'?')) || (at = strchr(encoded,'#')))
  {
    uri->canonical = strndup(encoded, (size_t)(at - encoded));
  }else{
    uri->canonical = strdup(encoded);
  }
  util_uri_canonical(uri,NULL);

  // optional session
  if((at = strchr(encoded,'/')))
  {
    encoded = at+1;
    if((at = strchr(encoded,'?')) || (at = strchr(encoded,'#')))
    {
      uri->session = strndup(encoded, (size_t)(at - encoded));
    }else{
      uri->session = strdup(encoded);
    }
  }

  // optional token at the end
  if((at = strchr(encoded,'#')))
  {
    uri->token = strdup(at+1);
  }

  // optional keys query string
  if((at = strchr(encoded,'?')))
  {
    uri->keys = lob_new();
    encoded = at+1;
    if((at = strchr(encoded,'#')))
    {
      klen = (size_t)(at - encoded);
    }else{
      klen = strlen(encoded);
    }
    while(klen)
    {
      // skip any separator
      if(*encoded == '&')
      {
        encoded++;
        klen--;
      }
      // require the equals
      if(!(val = strchr(encoded,'='))) break;
      val++;
      if((at = strchr(val,'&')))
      {
        vlen = (size_t)(at - val);
      }else{
        vlen = strlen(val);
      }
      lob_set_raw(uri->keys, encoded, (size_t)(val-encoded)-1, val, vlen);
      // skip past whole block
      klen -= (size_t)((val+vlen) - encoded);
      encoded = val + vlen;
    }

    // make sure at least one key/value was given
    if(!lob_keys(uri->keys)) uri->keys = lob_free(uri->keys);
  }

  return uri;
}

util_uri_t util_uri_free(util_uri_t uri)
{
  if(!uri) return NULL;
  free(uri->protocol);
  free(uri->canonical);
  free(uri->address);
  if(uri->user) free(uri->user);
  if(uri->session) free(uri->session);
  if(uri->token) free(uri->token);
  lob_free(uri->keys);
  free(uri);
  return NULL;
}

// produces string safe to use until next encode or free
char *util_uri_encode(util_uri_t uri)
{
  uint32_t len, i;
  char *key;

  if(!uri) return LOG("bad args");
  if(uri->encoded) free(uri->encoded);
  len = 9; // space for seperator chars of '://@:/?#\0'
  len += strlen(uri->protocol);
  len += uri->user?strlen(uri->user):0;
  len += strlen(uri->address);
  len += 11;
  len += uri->session?strlen(uri->session):0;
  len += uri->keys?lob_len(uri->keys):0;
  len += uri->token?strlen(uri->token):0;
  if(!(uri->encoded = malloc(len))) return LOG("OOM %d",len);
  memset(uri->encoded,0,len);

  sprintf(uri->encoded,"%s://",uri->protocol);
  if(uri->user) sprintf(uri->encoded+strlen(uri->encoded),"%s@",uri->user);
  sprintf(uri->encoded+strlen(uri->encoded),"%s",uri->address);
  if(uri->port) sprintf(uri->encoded+strlen(uri->encoded),":%u",uri->port);
  sprintf(uri->encoded+strlen(uri->encoded),"/%s",uri->session?uri->session:"");
  if(uri->keys)
  {
    lob_sort(uri->keys);
    sprintf(uri->encoded+strlen(uri->encoded),"?");
    i = 0;
    while((key = lob_get_index(uri->keys, i)))
    {
      sprintf(uri->encoded+strlen(uri->encoded),"%s%s=%s",i?"&":"", key, lob_get_index(uri->keys, i+1));
      i += 2;
    }
  }
  if(uri->token) sprintf(uri->encoded+strlen(uri->encoded),"#%s",uri->token);
  return uri->encoded;
}

util_uri_t util_uri_protocol(util_uri_t uri, char *protocol)
{
  if(!uri || !protocol) return LOG("bad args");
  if(uri->protocol) free(uri->protocol);
  uri->protocol = strdup(protocol);
  return uri;
}

util_uri_t util_uri_user(util_uri_t uri, char *user)
{
  if(!uri) return LOG("bad args");
  if(uri->user) free(uri->user);
  uri->user = user?strdup(user):NULL;
  return uri;
}

util_uri_t util_uri_canonical(util_uri_t uri, char *canonical)
{
  char *at;
  if(!uri) return LOG("bad args");
  if(canonical)
  {
    if(uri->canonical) free(uri->canonical);
    uri->canonical = strdup(canonical);
  }

  if(uri->address) free(uri->address);
  
  if((at = strchr(uri->canonical,':')))
  {
    uri->address = strndup(uri->canonical, (size_t)(at - uri->canonical));
    uri->port = (uint32_t)strtoul(at+1,NULL,10);
  }else{
    uri->address = strdup(uri->canonical);
    uri->port = 0;
  }

  return uri;
}

util_uri_t util_uri_address(util_uri_t uri, char *address)
{
  if(!uri || !address) return LOG("bad args");
  if(uri->address) free(uri->address);
  uri->address = strdup(address);
  return uri;
}

util_uri_t util_uri_port(util_uri_t uri, uint32_t port)
{
  if(!uri) return LOG("bad args");
  uri->port = port;
  return uri;
}

util_uri_t util_uri_session(util_uri_t uri, char *session)
{
  if(!uri) return LOG("bad args");
  if(uri->session) free(uri->session);
  uri->session = session?strdup(session):NULL;
  return uri;
}

util_uri_t util_uri_keys(util_uri_t uri, lob_t keys)
{
  if(!uri || !keys) return LOG("bad args");
  if(uri->keys) lob_free(uri->keys);
  uri->keys = lob_copy(keys);
  return uri;
}

util_uri_t util_uri_token(util_uri_t uri, char *token)
{
  if(!uri) return LOG("bad args");
  if(uri->token) free(uri->token);
  uri->token = token?strdup(token):NULL;
  return uri;
}
 */
