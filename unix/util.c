#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include "util_unix.h"

#include "mesh.h"

lob_t util_fjson(char *file)
{
  unsigned char *buf;
  int len;
  struct stat fs;
  FILE *fd;
  lob_t p;
  
  fd = fopen(file,"rb");
  if(!fd) return LOG("fopen error %s: %s",file,strerror(errno));
  if(fstat(fileno(fd),&fs) < 0) return LOG("fstat error %s: %s",file,strerror(errno));
  
  buf = malloc(fs.st_size);
  len = fread(buf,1,fs.st_size,fd);
  fclose(fd);
  if(len != fs.st_size)
  {
    free(buf);
    return LOG("fread %d != %d for %s: %s",len,fs.st_size,file,strerror(errno));
  }
  
  p = lob_new();
  lob_head(p, buf, len);
  if(!p) return LOG("json failed %s parsing %.*s",file,len,buf);
  return p;
}

mesh_t util_links(mesh_t mesh, char *file)
{
  lob_t links = util_fjson(file);
  if(!links) return NULL;

  // TODO iterate and link

  return mesh;
}

