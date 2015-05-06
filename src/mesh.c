#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "util.h"

// a default prime number for the internal hashtable used to track all active hashnames/lines
#define MAXPRIME 4211

// internally handle list of triggers active on the mesh
typedef struct on_struct
{
  char *id; // used to store in index
  
  void (*free)(mesh_t mesh); // relese resources
  void (*link)(link_t link); // when a link is created, and again when exchange is created
  pipe_t (*path)(link_t link, lob_t path); // convert path->pipe
  lob_t (*open)(link_t link, lob_t open); // incoming channel requests
  link_t (*discover)(mesh_t mesh, lob_t discovered, pipe_t pipe); // incoming unknown hashnames
  
  struct on_struct *next;
} *on_t;
on_t on_get(mesh_t mesh, char *id);
on_t on_free(on_t on);


mesh_t mesh_new(uint32_t prime)
{
  mesh_t mesh;
  
  // make sure we've initialized
  if(e3x_init(NULL)) return LOG("e3x init failed");

  if(!(mesh = malloc(sizeof (struct mesh_struct)))) return NULL;
  memset(mesh, 0, sizeof(struct mesh_struct));
  mesh->index = xht_new(prime?prime:MAXPRIME);
  if(!mesh->index) return mesh_free(mesh);
  
  LOG("mesh created version %d.%d.%d",TELEHASH_VERSION_MAJOR,TELEHASH_VERSION_MINOR,TELEHASH_VERSION_PATCH);

  return mesh;
}

mesh_t mesh_free(mesh_t mesh)
{
  on_t on;
  if(!mesh) return NULL;

  // free any triggers first
  while(mesh->on)
  {
    on = mesh->on;
    mesh->on = on->next;
    if(on->free) on->free(mesh);
    free(on->id);
    free(on);
  }

  xht_free(mesh->index);
  lob_free(mesh->keys);
  lob_free(mesh->paths);
  e3x_self_free(mesh->self);
  if(mesh->uri) free(mesh->uri);
  if(mesh->ipv4_local) free(mesh->ipv4_local);
  if(mesh->ipv4_public) free(mesh->ipv4_public);

  free(mesh);
  return NULL;
}

// must be called to initialize to a hashname from keys/secrets, return !0 if failed
uint8_t mesh_load(mesh_t mesh, lob_t secrets, lob_t keys)
{
  if(!mesh || !secrets || !keys) return 1;
  if(!(mesh->self = e3x_self_new(secrets, keys))) return 2;
  mesh->keys = lob_copy(keys);
  mesh->id = hashname_keys(mesh->keys);
  LOG("mesh is %s",mesh->id->hashname);
  return 0;
}

// creates a new mesh identity, returns secrets
lob_t mesh_generate(mesh_t mesh)
{
  lob_t secrets;
  if(!mesh || mesh->self) return LOG("invalid mesh");
  secrets = e3x_generate();
  if(!secrets) return LOG("failed to generate %s",e3x_err());
  if(mesh_load(mesh, secrets, lob_linked(secrets))) return lob_free(secrets);
  return secrets;
}

// return the best current URI to this endpoint, optional base uri
char *mesh_uri(mesh_t mesh, char *base)
{
  lob_t uri;
  if(!mesh) return LOG("bad args");

  // TODO use a router-provided base
  uri = util_uri_parse(base?base:"link://127.0.0.1");

  // set best current values
  util_uri_add_keys(uri, mesh->keys);
  if(mesh->port_local) lob_set_uint(uri, "port", mesh->port_local);
  if(mesh->port_public) lob_set_uint(uri, "port", mesh->port_public);
  if(mesh->ipv4_local) lob_set(uri, "hostname", mesh->ipv4_local);
  if(mesh->ipv4_public) lob_set(uri, "hostname", mesh->ipv4_public);

  // save/return new encoded output
  if(mesh->uri) free(mesh->uri);
  mesh->uri = strdup(util_uri_format(uri));
  lob_free(uri);
  return mesh->uri;
}

// generate json of mesh keys and current paths
lob_t mesh_json(mesh_t mesh)
{
  size_t len = 3; // []\0
  char *paths;
  lob_t json, path;
  if(!mesh) return LOG("bad args");

  json = lob_new();
  lob_set(json,"hashname",mesh->id->hashname);
  lob_set_raw(json,"keys",0,(char*)mesh->keys->head,mesh->keys->head_len);

  paths = malloc(len);
  sprintf(paths,"[");
  for(path = mesh->paths;path;path = lob_next(path))
  {
    len += path->head_len+1;
    paths = realloc(paths, len);
    sprintf(paths+strlen(paths),"%.*s,",(int)path->head_len,path->head);
  }
  if(len == 3)
  {
    sprintf(paths+strlen(paths),"]");
  }else{
    sprintf(paths+(strlen(paths)-1),"]");
  }
  lob_set_raw(json,"paths",0,paths,strlen(paths));
  free(paths);
  return json;
}

link_t mesh_add(mesh_t mesh, lob_t json, pipe_t pipe)
{
  link_t link;
  lob_t keys, paths;
  uint8_t csid;

  if(!mesh || !json) return LOG("bad args");
  LOG("mesh add %s",lob_json(json));
  link = link_get(mesh, lob_get(json,"hashname"));
  keys = lob_get_json(json,"keys");
  paths = lob_get_array(json,"paths");
  if(!link) link = link_keys(mesh, keys);
  if(!link) return LOG("no hashname");
  
  if(keys && (csid = hashname_id(mesh->keys,keys))) link_load(link, csid, keys);

  // handle any pipe/paths
  if(pipe) link_pipe(link, pipe);
  for(;paths;paths = paths->next) link_path(link,paths);

  return link;
}

link_t mesh_linked(mesh_t mesh, char *hashname)
{
  if(!mesh || !hashname_valid(hashname)) return NULL;
  
  return xht_get(mesh->index, hashname);
  
}

// create our generic callback linked list entry
on_t on_get(mesh_t mesh, char *id)
{
  on_t on;
  
  if(!mesh || !id) return LOG("bad args");
  for(on = mesh->on; on; on = on->next) if(util_cmp(on->id,id) == 0) return on;

  if(!(on = malloc(sizeof (struct on_struct)))) return LOG("OOM");
  memset(on, 0, sizeof(struct on_struct));
  on->id = strdup(id);
  on->next = mesh->on;
  mesh->on = on;
  return on;
}

void mesh_on_free(mesh_t mesh, char *id, void (*free)(mesh_t mesh))
{
  on_t on = on_get(mesh, id);
  if(on) on->free = free;
}

void mesh_on_path(mesh_t mesh, char *id, pipe_t (*path)(link_t link, lob_t path))
{
  on_t on = on_get(mesh, id);
  if(on) on->path = path;
}

pipe_t mesh_path(mesh_t mesh, link_t link, lob_t path)
{
  on_t on;
  pipe_t pipe = NULL;
  if(!mesh || !link || !path) return NULL;

  for(on = mesh->on; on; on = on->next)
  {
    if(on->path) pipe = on->path(link, path);
    if(pipe) return pipe;
  }
  return LOG("no pipe for path %.*s",path->head_len,path->head);
}

void mesh_on_link(mesh_t mesh, char *id, void (*link)(link_t link))
{
  on_t on = on_get(mesh, id);
  if(on) on->link = link;
}

void mesh_link(mesh_t mesh, link_t link)
{
  on_t on;
  for(on = mesh->on; on; on = on->next) if(on->link) on->link(link);
}

void mesh_on_open(mesh_t mesh, char *id, lob_t (*open)(link_t link, lob_t open))
{
  on_t on = on_get(mesh, id);
  if(on) on->open = open;
}

lob_t mesh_open(mesh_t mesh, link_t link, lob_t open)
{
  on_t on;
  for(on = mesh->on; open && on; on = on->next) if(on->open) open = on->open(link, open);
  return open;
}

void mesh_on_discover(mesh_t mesh, char *id, link_t (*discover)(mesh_t mesh, lob_t discovered, pipe_t pipe))
{
  on_t on = on_get(mesh, id);
  if(on) on->discover = discover;
}

void mesh_discover(mesh_t mesh, lob_t discovered, pipe_t pipe)
{
  on_t on;
  LOG("running mesh discover with %s",lob_json(discovered));
  for(on = mesh->on; on; on = on->next) if(on->discover) on->discover(mesh, discovered, pipe);
}

// add a custom outgoing handshake packet to all links
mesh_t mesh_handshake(mesh_t mesh, lob_t handshake)
{
  if(!mesh) return NULL;
  if(handshake && !lob_get(handshake,"type")) return LOG("handshake missing a type: %s",lob_json(handshake));
  mesh->handshakes = lob_link(handshake, mesh->handshakes);
  return mesh;
}

// query the cache of handshakes for a matching one with a specific type
lob_t mesh_handshakes(mesh_t mesh, lob_t handshake, char *type)
{
  lob_t matched;
  char *id;
  
  if(!mesh || !type || !(id = lob_get(handshake,"id"))) return LOG("bad args");
  
  for(matched = mesh->cached;matched;matched = lob_match(matched->next,"id",id))
  {
    if(lob_get_cmp(matched,"type",type) == 0) return matched;
  }
  
  return NULL;
}

// process any unencrypted handshake packet
link_t mesh_receive_handshake(mesh_t mesh, lob_t handshake, pipe_t pipe)
{
  uint32_t now;
  lob_t iter, tmp, outer;
  hashname_t from;
  char hexid[3], *paths;
  link_t link;

  if(!mesh || !handshake) return LOG("bad args");
  if(!lob_get(handshake,"id"))
  {
    lob_free(handshake);
    return LOG("bad handshake, no id");
  }
  now = util_sys_seconds();
  
  // normalize handshake
  handshake->id = now; // save when we cached it
  if(!lob_get(handshake,"type")) lob_set(handshake,"type","link"); // default to link type
  if(!lob_get_uint(handshake,"at")) lob_set_uint(handshake,"at",now); // require an at
  
  // validate/extend link handshakes immediately
  if(util_cmp(lob_get(handshake,"type"),"link") == 0 && (outer = lob_linked(handshake)))
  {
    // get attached hashname
    tmp = lob_parse(handshake->body, handshake->body_len);
    from = hashname_key(tmp, outer->head[0]);
    if(!from)
    {
      LOG("bad link handshake, no hashname: %s",lob_json(handshake));
      lob_free(tmp);
      lob_free(handshake);
      return NULL;
    }
    util_hex(outer->head, 1, hexid);
    lob_set(handshake,"csid",hexid);
    lob_set(handshake,"hashname",from->hashname);
    lob_body(handshake, tmp->body, tmp->body_len); // re-attach as raw key
    lob_free(tmp);
    hashname_free(from);

    // short-cut, if it's a key from an existing link, pass it on
    if((link = mesh_linked(mesh,lob_get(handshake,"hashname")))) return link_receive_handshake(link, handshake, pipe);
    
    // extend the key json to make it compatible w/ normal patterns
    tmp = lob_new();
    lob_set_base32(tmp,hexid,handshake->body,handshake->body_len);
    lob_set_raw(handshake,"keys",0,(char*)tmp->head,tmp->head_len);
    lob_free(tmp);
    // add the path if one
    if(pipe && pipe->path)
    {
      paths = malloc(pipe->path->head_len+3);
      sprintf(paths,"[%.*s]",(int)pipe->path->head_len,(char*)pipe->path->head);
      lob_set_raw(handshake,"paths",0,paths,pipe->path->head_len+2);
      free(paths);
    }
  }

  // always add to the front of the cached list if needed in the future
  mesh->cached = lob_unshift(mesh->cached, handshake);

  // tell anyone listening about the newly discovered handshake
  mesh_discover(mesh, handshake, pipe);
  
  // cull/gc the cache for anything older than 5 seconds
  iter = mesh->cached;
  while(iter)
  {
    tmp = iter;
    iter = iter->next;
    if(tmp->id >= (now-5)) continue; // 5 seconds
    mesh->cached = lob_splice(mesh->cached, tmp);
    lob_free(tmp);
  }

  return NULL;
}

// processes incoming packet, it will take ownership of p
uint8_t mesh_receive(mesh_t mesh, lob_t outer, pipe_t pipe)
{
  lob_t inner;
  link_t link;
  char hex[33];

  if(!mesh || !outer || !pipe)
  {
    LOG("bad args");
    return 1;
  }
  
  LOG("mesh receiving %s to %s via pipe %s",outer->head_len?"handshake":"channel",mesh->id->hashname,pipe->id);

  // process handshakes
  if(outer->head_len == 1)
  {
    inner = e3x_self_decrypt(mesh->self, outer);
    if(!inner)
    {
      LOG("%02x handshake failed %s",outer->head[0],e3x_err());
      lob_free(outer);
      return 2;
    }
    
    // couple the two together, inner->outer
    lob_link(inner,outer);

    // set the unique id string based on the first 16 (routing token) bytes in the body
    util_hex(outer->body,16,hex);
    lob_set(inner,"id",hex);

    // process the handshake
    return mesh_receive_handshake(mesh, inner, pipe) ? 0 : 3;
  }

  // handle channel packets
  if(outer->head_len == 0)
  {
    if(outer->body_len < 16)
    {
      LOG("packet too small %d",outer->body_len);
      return 5;
    }
    util_hex(outer->body, 16, hex);
    link = xht_get(mesh->index, hex);
    if(!link)
    {
      LOG("dropping, no link for token %s",hex);
      lob_free(outer);
      return 6;
    }

    inner = e3x_exchange_receive(link->x, outer);
    if(!inner)
    {
      LOG("channel decryption fail for link %s %s",link->id->hashname,e3x_err());
      lob_free(outer);
      return 7;
    }
    
    LOG("channel packet %d bytes from %s",lob_len(inner),link->id->hashname);
    return link_receive(link,inner,pipe) ? 0 : 8;
    
  }
  
  LOG("dropping unknown outer packet with header %d %s",outer->head_len,lob_json(outer));
  lob_free(outer);

  return 10;
}
