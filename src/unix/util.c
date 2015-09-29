#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))

#include <stdio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include "telehash.h"

#include "telehash.h"

lob_t util_fjson(char *file)
{
  unsigned char *buf;
  size_t len;
  struct stat fs;
  FILE *fd;
  lob_t p;
  
  fd = fopen(file,"rb");
  if(!fd) return LOG("fopen error %s: %s",file,strerror(errno));
  if(fstat(fileno(fd),&fs) < 0)
  {
    fclose(fd);
    return LOG("fstat error %s: %s",file,strerror(errno));
  }
  
  if(!(buf = malloc((size_t)fs.st_size)))
  {
    fclose(fd);
    return LOG("OOM");
  }
  len = fread(buf,1,(size_t)fs.st_size,fd);
  fclose(fd);
  if(len != (size_t)fs.st_size)
  {
    free(buf);
    return LOG("fread %d != %d for %s: %s",len,fs.st_size,file,strerror(errno));
  }
  
  p = lob_new();
  lob_head(p, buf, len);
  if(!p) LOG("json failed %s parsing %.*s",file,len,buf);
  free(buf);
  return p;
}

mesh_t util_links(mesh_t mesh, char *file)
{
  lob_t links = util_fjson(file);
  if(!links) return NULL;

  // TODO iterate and link

  lob_free(links);

  return mesh;
}

int util_sock_timeout(int sock, uint32_t ms)
{
  struct timeval tv;

  tv.tv_sec = ms/1000;
  tv.tv_usec = (ms%1000)*1000;

  if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
  {
    LOG("timeout setsockopt error %s",strerror(errno));
    return -1;
  }

  return sock;
}

#endif
