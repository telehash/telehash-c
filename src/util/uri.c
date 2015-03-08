#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "util.h"

// this is a very simple single-pass telehash uri parser, no magic
lob_t util_uri_parse(char *encoded)
{
  size_t klen, vlen;
  char *at, *val;
  lob_t uri;
  
  if(!encoded) return LOG("bad args");
  uri = lob_new();

  // check for protocol:// prefix first
  if(!(at = strstr(encoded,"://")))
  {
    lob_set(uri, "protocol", "link");
  }else{
    lob_set_len(uri, "protocol", 0, encoded, (size_t)(at - encoded));
    encoded = at+3;
  }
  
  // check for user@ prefix next
  if((at = strchr(encoded,'@')))
  {
    lob_set_len(uri, "auth", 0, encoded, (size_t)(at - encoded));
    encoded = at+1;
  }
  
  // ensure there's at least a host
  if(!strlen(encoded) || !isalnum(encoded[0])) return LOG("invalid host: '%s'",encoded);

  // copy in host and parse hostname/port
  if((at = strchr(encoded,'/')) || (at = strchr(encoded,'?')) || (at = strchr(encoded,'#')))
  {
    lob_set_len(uri, "host", 0, encoded, (size_t)(at - encoded));
  }else{
    lob_set_len(uri, "host", 0, encoded, strlen(encoded));
  }

  // hostname+port
  val = lob_get(uri, "host");
  if(val && (at = strchr(val,':')))
  {
    lob_set_len(uri, "hostname", 0, val, (size_t)(at - val));
    lob_set_uint(uri, "port", (uint16_t)strtoul(at+1,NULL,10));
  }else{
    lob_set(uri, "hostname", val);
  }

  // optional path
  if((at = strchr(encoded,'/')))
  {
    if((at = strchr(at+1,'?')) || (at = strchr(at+1,'#')))
    {
      lob_set_len(uri, "path", 0, encoded, (size_t)(at - encoded));
    }else{
      lob_set_len(uri, "path", 0, encoded, strlen(encoded));
    }
  }

  // optional hash at the end
  if((at = strchr(encoded,'#')))
  {
    lob_set_len(uri, "path", 0, at+1, strlen(at+1));
  }

  // optional query string
  if((at = strchr(encoded,'?')))
  {
    uri->chain = lob_new();
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
      lob_set_len(uri->chain, encoded, (size_t)(val-encoded)-1, val, vlen);
      // skip past whole block
      klen -= (size_t)((val+vlen) - encoded);
      encoded = val + vlen;
    }
  }

  return uri;
}

// get keys from query
lob_t util_uri_keys(lob_t uri)
{
  uint32_t i;
  char *key, *value;
  lob_t keys, query = lob_linked(uri);
  if(!query) return NULL;
  keys = lob_new();

  // loop through all keyval pairs to find cs**
  for(i=0;(key = lob_get_index(query,i));i+=2)
  {
    value = lob_get_index(query,i+1);
    if(strlen(key) != 4 || strncmp(key,"cs",2) != 0 || !value) continue; // skip non-csid keys
    lob_set_len(keys,key+2,2,value,strlen(value));
  }
  
  return keys;
}

// get paths from host and query
lob_t util_uri_paths(lob_t uri)
{
  uint32_t i;
  uint16_t port;
  char *key, *value;
  lob_t paths, query = lob_linked(uri);
  if(!query) return NULL;
  paths = NULL;
  
  // gen paths from host/port
  if((port = lob_get_uint(uri,"port")))
  {
    key = lob_get(uri,"host");
    paths = lob_chain(paths);
    lob_set(paths,"type","upd4");
    lob_set(paths,"ip",key);
    lob_set_uint(paths,"port",port);
    paths = lob_chain(paths);
    lob_set(paths,"type","tcp4");
    lob_set(paths,"ip",key);
    lob_set_uint(paths,"port",port);
  }

  // loop through all keyval pairs to find paths
  for(i=0;(key = lob_get_index(query,i));i+=2)
  {
    value = lob_get_index(query,i+1);
    if(util_cmp(key,"paths") != 0 || !value) continue;
    
  }
  
  return paths;
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
