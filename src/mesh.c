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

// return the best current URI to this endpoint, optional override protocol
char *mesh_uri(mesh_t mesh, char *protocol)
{
  util_uri_t uri;
  if(!mesh) return LOG("bad args");

  // load existing or create new
  uri = (mesh->uri) ? util_uri_new(mesh->uri, protocol) : util_uri_new("127.0.0.1", protocol);
  
  // TODO don't override a router-provided base

  // set best current values
  util_uri_keys(uri, mesh->keys);
  if(mesh->port_local) util_uri_port(uri, mesh->port_local);
  if(mesh->port_public) util_uri_port(uri, mesh->port_public);
  if(mesh->ipv4_local) util_uri_address(uri, mesh->ipv4_local);
  if(mesh->ipv4_public) util_uri_address(uri, mesh->ipv4_public);

  // save/return new encoded output
  if(mesh->uri) free(mesh->uri);
  mesh->uri = strdup(util_uri_encode(uri));
  util_uri_free(uri);
  return mesh->uri;
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

  return NULL;
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
  pipe_t pipe;
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
  for(on = mesh->on; on; on = on->next) if(on->discover) on->discover(mesh, discovered, pipe);
}

// processes incoming packet, it will take ownership of p
uint8_t mesh_receive(mesh_t mesh, lob_t outer, pipe_t pipe)
{
  lob_t inner, discovered, key;
  hashname_t from;
  link_t link;
  char hex[33], *paths;

  if(!mesh || !outer || !pipe)
  {
    LOG("bad args");
    return 1;
  }
  
  LOG("mesh receiving %s to %s via pipe %s",outer->head_len?"handshake":"channel",mesh->id->hashname,pipe->id);

  // process handshakes
  if(outer->head_len == 1)
  {
    util_hex(outer->head,1,hex);
    inner = e3x_self_decrypt(mesh->self, outer);
    if(!inner)
    {
      LOG("%s handshake failed %s",hex,e3x_err());
      lob_free(outer);
      return 2;
    }
    
    // couple the two together, outer->inner
    lob_link(outer,inner);

    // make sure csid is set on the handshake to get the hashname
    lob_set_raw(inner,hex,0,"true",4);
    from = hashname_key(inner);
    if(!from)
    {
      LOG("no hashname in %.*s",inner->head_len,inner->head);
      lob_free(outer);
      return 2;
      
    }
    
    link = xht_get(mesh->index,from->hashname);
    if(!link)
    {
      LOG("no link for hashname %s",from->hashname);
      // serialize all the new hashname's info into json for the app to access/handle
      discovered = lob_new();
      lob_set(discovered,"hashname",from->hashname);
      // add the key
      key = lob_new();
      lob_set_base32(key,hex,inner->body,inner->body_len);
      lob_set_raw(discovered,"keys",0,(char*)key->head,key->head_len);
      lob_free(key);
      // add the path if one
      if(pipe && pipe->path)
      {
        paths = malloc(pipe->path->head_len+2);
        sprintf(paths,"[%s]",lob_json(pipe->path));
        lob_set_raw(discovered,"paths",0,paths,pipe->path->head_len+2);
        free(paths);
      }
      mesh_discover(mesh, discovered, pipe);

      link = xht_get(mesh->index,from->hashname);
      if (!link) {
        hashname_free(from);
        lob_free(outer);
        return 3;
      }
    }
    hashname_free(from);

    LOG("incoming handshake for link %s",link->id->hashname);
    return link_handshake(link,inner,outer,pipe) ? 0 : 4;
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
  
  LOG("dropping unknown outer packet with header %d %s",outer->head_len,util_hex(outer->head,outer->head_len,NULL));
  lob_free(outer);

  return 10;
}
