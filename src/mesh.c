#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "telehash.h"

// a default prime number for the internal hashtable used by extensions
#define MAXPRIME 11

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

// internally handle list of forwarding routes for the mesh
typedef struct route_struct
{
  uint8_t flag;
  uint8_t token[8];
  link_t link;
  
  struct route_struct *next;
} *route_t;

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

  // free all links first
  link_t link, next;
  for(link = mesh->links;link;link = next)
  {
    next = link->next;
    link_free(link);
  }
  
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
  lob_freeall(mesh->cached);
  hashname_free(mesh->id);
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
  mesh->id = hashname_dup(hashname_vkeys(mesh->keys));
  LOG("mesh is %s",hashname_short(mesh->id));
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
  lob_t json, paths;
  if(!mesh) return LOG("bad args");

  json = lob_new();
  lob_set(json,"hashname",hashname_char(mesh->id));
  lob_set_raw(json,"keys",0,(char*)mesh->keys->head,mesh->keys->head_len);
  paths = lob_array(mesh->paths);
  lob_set_raw(json,"paths",0,(char*)paths->head,paths->head_len);
  lob_free(paths);
  return json;
}

// generate json for all links, returns lob list
lob_t mesh_links(mesh_t mesh)
{
  lob_t links = NULL;
  link_t link;

  for(link = mesh->links;link;link = link->next)
  {
    links = lob_push(links,link_json(link));
  }
  return links;
}

// process any channel timeouts based on the current/given time
mesh_t mesh_process(mesh_t mesh, uint32_t now)
{
  link_t link, next;
  if(!mesh || !now) return LOG("bad args");
  for(link = mesh->links;link;link = next)
  {
    next = link->next;
    link_process(link, now);
  }
  
  // cull/gc the cache for anything older than 5 seconds
  lob_t iter = mesh->cached;
  while(iter)
  {
    lob_t tmp = iter;
    iter = lob_next(iter);
    if(tmp->id >= (now-5)) continue; // 5 seconds
    mesh->cached = lob_splice(mesh->cached, tmp);
    lob_free(tmp);
  }
  
  return mesh;
}

link_t mesh_add(mesh_t mesh, lob_t json, pipe_t pipe)
{
  link_t link;
  lob_t keys, paths;
  uint8_t csid;

  if(!mesh || !json) return LOG("bad args");
  LOG("mesh add %s",lob_json(json));
  link = link_get(mesh, hashname_vchar(lob_get(json,"hashname")));
  keys = lob_get_json(json,"keys");
  paths = lob_get_array(json,"paths");
  if(!link) link = link_keys(mesh, keys);
  if(!link) LOG("no hashname");
  
  LOG("loading keys/paths");
  if(keys && (csid = hashname_id(mesh->keys,keys))) link_load(link, csid, keys);

  // handle any pipe/paths
  if(pipe) link_pipe(link, pipe);
  lob_t path;
  for(path=paths;path;path = lob_next(path)) link_path(link,path);
  
  lob_free(keys);
  lob_freeall(paths);

  return link;
}

link_t mesh_linked(mesh_t mesh, char *hn, size_t len)
{
  link_t link;
  if(!mesh || !hn) return NULL;
  if(!len) len = strlen(hn);
  
  for(link = mesh->links;link;link = link->next) if(strncmp(hashname_char(link->id),hn,len) == 0) return link;
  
  return NULL;
}

link_t mesh_linkid(mesh_t mesh, hashname_t id)
{
  link_t link;
  if(!mesh || !id) return NULL;
  
  for(link = mesh->links;link;link = link->next) if(hashname_cmp(link->id,id) == 0) return link;
  
  return NULL;
}

// remove this link, will event it down and clean up during next process()
mesh_t mesh_unlink(link_t link)
{
  if(!link) return NULL;
  link->csid = 0; // removal indicator
  return link->mesh;
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
  // set up token forwarding w/ flag
  mesh_forward(mesh, e3x_exchange_token(link->x), link, 1);

  // event notifications
  on_t on;
  for(on = mesh->on; on; on = on->next) if(on->link) on->link(link);
}

mesh_t mesh_forward(mesh_t mesh, uint8_t *token, link_t link, uint8_t flag)
{
  // set up route for link's token
  route_t r, empty = NULL;
  
  // ignore any existing routes
  uint8_t max=0;
  for(r=mesh->routes;r;r=r->next)
  {
    if(memcmp(token,r->token,8) == 0) return mesh;
    if(flag && r->link == link) r->link = NULL; // clear any existing for this link when flagged
    if(!empty && !r->link) empty = r;
    max++;
  }

  // create new route
  if(!empty)
  {
    if(max > 32)
    {
      LOG("too many routes, can't add forward (TODO: add eviction)");
      return NULL;
    }
    LOG("adding new forwarding route to %s token %s",hashname_short(link->id),util_hex(token,8,NULL));
    empty = malloc(sizeof(struct route_struct));
    memset(empty,0,sizeof(struct route_struct));
    empty->next = mesh->routes;
    mesh->routes = empty;
  }
  
  memcpy(empty->token,token,8);
  empty->link = link;
  empty->flag = flag;
  
  return mesh;
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
  hashname_t from;
  link_t link;

  if(!mesh || !handshake) return LOG("bad args");
  if(!lob_get(handshake,"id"))
  {
    LOG("bad handshake, no id: %s",lob_json(handshake));
    lob_free(handshake);
    return NULL;
  }
  now = util_sys_seconds();
  
  // normalize handshake
  handshake->id = now; // save when we cached it
  if(!lob_get(handshake,"type")) lob_set(handshake,"type","link"); // default to link type
  if(!lob_get_uint(handshake,"at")) lob_set_uint(handshake,"at",now); // require an at
  LOG("handshake at %d id %s",now,lob_get(handshake,"id"));
  
  // validate/extend link handshakes immediately
  if(util_cmp(lob_get(handshake,"type"),"link") == 0)
  {
    // get the csid
    uint8_t csid = 0;
    lob_t outer;
    if((outer = lob_linked(handshake)))
    {
      csid = outer->head[0];
    }else if(lob_get(handshake,"csid")){
      util_unhex(lob_get(handshake,"csid"),2,&csid);
    }
    if(!csid)
    {
      LOG("bad link handshake, no csid: %s",lob_json(handshake));
      lob_free(handshake);
      return NULL;
    }
    char hexid[3] = {0};
    util_hex(&csid, 1, hexid);
      
    // get attached hashname
    lob_t tmp = lob_parse(handshake->body, handshake->body_len);
    from = hashname_vkey(tmp, csid);
    if(!from)
    {
      LOG("bad link handshake, no hashname: %s",lob_json(handshake));
      lob_free(tmp);
      lob_free(handshake);
      return NULL;
    }
    lob_set(handshake,"csid",hexid);
    lob_set(handshake,"hashname",hashname_char(from));
    lob_body(handshake, tmp->body, tmp->body_len); // re-attach as raw key
    lob_free(tmp);

    // short-cut, if it's a key from an existing link, pass it on
    // TODO: using mesh_linked here is a stack issue during loopback peer test!
    if((link = mesh_linkid(mesh,from))) return link_receive_handshake(link, handshake, pipe);
    LOG("no link found for handshake from %s",hashname_char(from));

    // extend the key json to make it compatible w/ normal patterns
    tmp = lob_new();
    lob_set_base32(tmp,hexid,handshake->body,handshake->body_len);
    lob_set_raw(handshake,"keys",0,(char*)tmp->head,tmp->head_len);
    lob_free(tmp);
    // add the path if one
    if(pipe && pipe->path)
    {
      char *paths = malloc(pipe->path->head_len+3);
      sprintf(paths,"[%.*s]",(int)pipe->path->head_len,(char*)pipe->path->head);
      lob_set_raw(handshake,"paths",0,paths,pipe->path->head_len+2);
      free(paths);
    }
  }

  // always add to the front of the cached list if needed in the future
  mesh->cached = lob_unshift(mesh->cached, handshake);

  // tell anyone listening about the newly discovered handshake
  mesh_discover(mesh, handshake, pipe);
  
  return NULL;
}

// processes incoming packet, it will take ownership of p
link_t mesh_receive(mesh_t mesh, lob_t outer, pipe_t pipe)
{
  lob_t inner;
  link_t link;
  char token[17];
  hashname_t id;

  if(!mesh || !outer || !pipe) return LOG("bad args");
  
  LOG("mesh receiving %s to %s via pipe %s",outer->head_len?"handshake":"channel",hashname_short(mesh->id),pipe->id);

  // process handshakes
  if(outer->head_len == 1)
  {
    inner = e3x_self_decrypt(mesh->self, outer);
    if(!inner)
    {
      LOG("%02x handshake failed %s",outer->head[0],e3x_err());
      lob_free(outer);
      return NULL;
    }
    
    // couple the two together, inner->outer
    lob_link(inner,outer);

    // set the unique id string based on some of the first 16 (routing token) bytes in the body
    base32_encode(outer->body,10,token,17);
    lob_set(inner,"id",token);

    // process the handshake
    return mesh_receive_handshake(mesh, inner, pipe);
  }

  // handle channel packets
  if(outer->head_len == 0)
  {
    if(outer->body_len < 16)
    {
      LOG("packet too small %d",outer->body_len);
      lob_free(outer);
      return NULL;
    }
    
    route_t route;
    for(route = mesh->routes;route;route = route->next) if(memcmp(route->token,outer->body,8) == 0) break;
    link = route ? route->link : NULL;
    if(!link)
    {
      LOG("dropping, no link for token %s",util_hex(outer->body,16,NULL));
      lob_free(outer);
      return NULL;
    }
    
    // forward packet
    if(!route->flag)
    {
      LOG("forwarding route token %s via %s len %d",util_hex(route->token,8,NULL),hashname_short(link->id),lob_len(outer));
      return link_send(link, outer);
    }
    
    inner = e3x_exchange_receive(link->x, outer);
    lob_free(outer);
    if(!inner) return LOG("channel decryption fail for link %s %s",hashname_short(link->id),e3x_err());
    
    LOG("channel packet %d bytes from %s",lob_len(inner),hashname_short(link->id));
    return link_receive(link,inner,pipe);
    
  }

  // transform incoming bare link json format into handshake for discovery
  if((inner = lob_get_json(outer,"keys")))
  {
    if((id = hashname_vkeys(inner)))
    {
      lob_set(outer,"hashname",hashname_char(id));
      lob_set_int(outer,"at",0);
      lob_set(outer,"type","link");
      LOG("bare incoming link json being discovered %s",lob_json(outer));
    }
    lob_free(inner);
  }
  
  // run everything else through discovery, usually plain handshakes
  mesh_discover(mesh, outer, pipe);
  link = mesh_linked(mesh, lob_get(outer,"hashname"), 0);
  lob_free(outer);

  return link;
}
