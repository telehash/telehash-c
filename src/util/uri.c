#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "telehash.h"

// this is a very simple single-pass telehash uri parser, no magic
lob_t util_uri_parse(char *encoded)
{
  size_t klen, vlen;
  char *at, *val;
  lob_t uri;
  
  if(!encoded) return LOG("bad args");
  uri = lob_new();
  lob_set(uri,"orig",encoded);

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
  if(!strlen(encoded) || !isalnum((int)encoded[0]))
  {
    lob_free(uri);
    return LOG("invalid host: '%s'",encoded);
  }

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
    encoded = at;
    if((at = strchr(encoded+1,'?')) || (at = strchr(encoded+1,'#')))
    {
      lob_set_len(uri, "path", 0, encoded, (size_t)(at - encoded));
    }else{
      lob_set_len(uri, "path", 0, encoded, strlen(encoded));
    }
  }

  // optional hash at the end
  if((at = strchr(encoded,'#')))
  {
    lob_set_len(uri, "hash", 0, at+1, strlen(at+1));
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
  uint8_t *buf;
  size_t len;
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
  buf = NULL;
  for(i=0;(key = lob_get_index(query,i));i+=2)
  {
    value = lob_get_index(query,i+1);
    if(util_cmp(key,"paths") != 0 || !value) continue;
    len = base32_decode_floor(strlen(value));
    buf = util_reallocf(buf,len);
    if(!buf) continue;
    if(base32_decode(value,strlen(value),buf,len) < len) continue;
    paths = lob_link(lob_parse(buf,len), paths);
  }
  free(buf);
  
  return paths;
}

// validate any fragment from this peer
uint8_t util_uri_check(lob_t uri, uint8_t *peer)
{
  return 0;
}

lob_t util_uri_add_keys(lob_t uri, lob_t keys)
{
  uint32_t i;
  char *key, *value, csid[5];
  lob_t query = lob_linked(uri);
  if(!uri || !keys) return NULL;
  if(!query)
  {
    query = lob_new();
    lob_link(uri, query);
  }
  
  for(i=0;(key = lob_get_index(keys,i));i+=2)
  {
    value = lob_get_index(keys,i+1);
    if(strlen(key) != 2 || !value) continue; // paranoid
    snprintf(csid,5,"cs%s",key);
    lob_set(query,csid,value);
  }

  return uri;
}

lob_t util_uri_add_path(lob_t uri, lob_t path)
{
  lob_t keys;
  lob_t query = lob_linked(uri);
  if(!uri || !path) return NULL;
  if(!query)
  {
    if(!(query = lob_new())) return LOG("OOM");
    lob_link(uri, query);
  }
  // encode and add to chain after query
  if(!(keys = lob_new())) return LOG("OOM");
  lob_set_base32(keys,"paths",path->head,path->head_len);
  lob_link(keys, lob_linked(query));
  lob_link(query, keys);
  
  return uri;
}

lob_t util_uri_add_check(lob_t uri, uint8_t *peer, uint8_t *data, size_t len)
{
  return NULL;
}

lob_t util_uri_add_data(lob_t uri, uint8_t *data, size_t len)
{
  return NULL;
}


// serialize out from lob format to "uri" key and return it
char *util_uri_format(lob_t uri)
{
  char *part, *key, *value;
  uint32_t i, prev = 0;
  lob_t buf, query;
  if(!uri) return NULL;
  
  // use a lob body as the buffer to build it up
  buf = lob_new();
  
  part = lob_get(uri, "protocol");
  if(part)
  {
    lob_append_str(buf, part);
  }else{
    lob_append_str(buf, "link");
  }
  lob_append_str(buf, "://");

  part = lob_get(uri, "hostname");
  if(part)
  {
    lob_append_str(buf, part);
    part = lob_get(uri, "port");
    if(part)
    {
      lob_append_str(buf, ":");
      lob_append_str(buf, part);
    }
  }else{
    part = lob_get(uri, "host");
    if(part) lob_append_str(buf, part);
  }
  
  part = lob_get(uri, "path");
  if(part)
  {
    lob_append_str(buf, part);
  }else{
    lob_append_str(buf, "/");
  }
  
  // append on any query string
  
  for(query = lob_linked(uri); query; query = lob_linked(query))
  {
    for(i=0;(key = lob_get_index(query,i));i+=2)
    {
      value = lob_get_index(query,i+1);
      if(!strlen(key) || !value) continue; // paranoid
      lob_append_str(buf,(prev++)?"&":"?");
      lob_append_str(buf,key);
      lob_append_str(buf,"=");
      lob_append_str(buf,value);
    }
  }

  if((part = lob_get(uri, "hash")))
  {
    lob_append_str(buf, "#");
    lob_append_str(buf, part);
  }

  lob_set_len(uri,"uri",3,(char*)buf->body,buf->body_len);
  lob_free(buf);

  return lob_get(uri,"uri");
}

