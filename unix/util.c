#include <string.h>
#include "util_unix.h"

void path2sa(path_t p, struct sockaddr_in *sa)
{
  if(util_cmp("ipv4",p->type)==0) inet_aton(p->ip, &(sa->sin_addr));
  sa->sin_port = htons(p->port);
}

void sa2path(struct sockaddr_in *sa, path_t p)
{
  strcpy(p->ip,inet_ntoa(sa->sin_addr));
  p->port = ntohs(sa->sin_port);
}
