#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <strings.h>
#include "util.h"

packet_t util_file2packet(char *file)
{
  unsigned char *buf;
  int len;
  struct stat fs;
  FILE *fd;
  packet_t p;
  
  fd = fopen(file,"rb");
  if(!fd) return NULL;
  if(fstat(fileno(fd),&fs) < 0) return NULL;
  
  buf = malloc(fs.st_size);
  len = fread(buf,1,fs.st_size,fd);
  fclose(fd);
  if(len != fs.st_size)
  {
    free(buf);
    return NULL;
  }
  
  p = packet_new();
  packet_json(p, buf, len);
  return p;
}

unsigned char *util_hex(unsigned char *in, int len, unsigned char *out)
{
    int j;
    unsigned char *c = out;

    for (j = 0; j < len; j++) {
      sprintf((char*)c,"%02x", in[j]);
      c += 2;
    }
    *c = '\0';
    return out;
}

unsigned char hexcode(unsigned char x)
{
    if (x >= '0' && x <= '9')         /* 0-9 is offset by hex 30 */
      return (x - 0x30);
    else if (x >= 'A' && x <= 'F')    /* A-F offset by hex 37 */
      return(x - 0x37);
    else if (x >= 'a' && x <= 'f')    /* a-f offset by hex 37 */
      return(x - 0x57);
    else {                            /* Otherwise, an illegal hex digit */
		return x;
    }
}

unsigned char *util_unhex(unsigned char *in, int len, unsigned char *out)
{
	int j;
	unsigned char *c = out;
	for(j=0; (j+1)<len; j+=2)
	{
		*c = ((hexcode(in[j]) * 16) & 0xF0) + (hexcode(in[j+1]) & 0xF);
		c++;
	}
	return out;
}

int util_cmp(char *a, char *b)
{
  if(!a || !b) return -1;
  return strcasecmp(a,b);
}
