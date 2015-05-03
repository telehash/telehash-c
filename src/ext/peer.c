#include "ext.h"

// handle incoming peer request to route
lob_t peer_on_open(link_t link, lob_t open)
{
  if(!link) return open;
  if(lob_get_cmp(open,"type","peer")) return open;
  
  LOG("incoming peer route request %s",lob_json(open));

  // TODO
  // find/verify destination link
  // generate connect request to them
  

  return NULL;
}
