#include "util.h"
#include "crypt.h"

struct crypt_struct
{
  char csid;
  unsigned char hashname[32];
  int private, lined;
  unsigned long atOut, atIn;
  unsigned char lineOut[16], lineIn[16], lineHex[33];
  void *cs; // for CS usage
};

char *crypt_line(crypt_t c)
{
  if(!c || !c->lined) return NULL;
  return (char*)c->lineHex;
}

char crypt_csid(crypt_t c)
{
  if(!c) return 0;
  return c->csid;
}

int crypt_init()
{
  int ret = -1;
#ifdef CS_1a
  ret = crypt_init_1a();
  if(ret) return ret;
#endif
#ifdef CS_2a
  ret = crypt_init_2a();
  if(ret) return ret;
#endif
#ifdef CS_3a
  ret = crypt_init_3a();
  if(ret) return ret;
#endif
  return ret;
}

crypt_t crypt_new(char csid, unsigned char *key, int len)
{
  if(!csid || !key || !len) return 0;
#ifdef CS_1a
  if(csid == 0x1a) return crypt_new_1a(key, len);
#endif
#ifdef CS_2a
  if(csid == 0x2a) return crypt_new_2a(key, len);
#endif
#ifdef CS_3a
  if(csid == 0x3a) return crypt_new_3a(key, len);
#endif
  return 0;
}

int crypt_public(crypt_t c, unsigned char *key, int len)
{
  if(!c || !der || !len) return 0;
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_public_1a(c, key, len);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_public_2a(c, key, len);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_public_3a(c, key, len);
#endif
  return 0;
}

void crypt_free(crypt_t c)
{
  if(!c) return;
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_free_1a(c);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_free_2a(c);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_free_3a(c);
#endif
  free(c);
}

int crypt_private(crypt_t c, unsigned char *key, int len)
{
  int ret;
  if(!c) return 1;
  if(c->private) return 0; // already loaded

#ifdef CS_1a
  if(c->csid == 0x1a && (ret = crypt_private_1a(c,key,len))) return ret;
#endif
#ifdef CS_2a
  if(c->csid == 0x2a && (ret = crypt_private_2a(c,key,len))) return ret;
#endif
#ifdef CS_3a
  if(c->csid == 0x3a && (ret = crypt_private_3a(c,key,len))) return ret;
#endif
  
  c->private = 1;
  return 0;
}

packet_t crypt_lineize(crypt_t self, crypt_t c, packet_t p)
{
  if(!self || !c || !p || !c->lined || self->csid != c->csid) return NULL;
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_lineize_1a(self,c,p);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_lineize_2a(self,c,p);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_lineize_3a(self,c,p);
#endif
  return NULL;
}

packet_t crypt_delineize(crypt_t self, crypt_t c, packet_t p)
{
  if(!self || !c || !p || self->csid != c->csid) return NULL;
  if(!c->lined) return packet_free(p);
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_delineize_1a(self,c,p);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_delineize_2a(self,c,p);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_delineize_3a(self,c,p);
#endif
  return NULL;
}

int crypt_lineinit(crypt_t c)
{
  if(!c) return 0;
#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_lineinit_1a(c);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_lineinit_2a(c);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_lineinit_3a(c);
#endif
  return 0;
}

packet_t crypt_openize(crypt_t self, crypt_t c)
{
  if(!c || !self || self->csid != c->csid) return NULL;

  // ensure line is setup
  if(crypt_lineinit(c)) return NULL;

#ifdef CS_1a
  if(c->csid == 0x1a) return crypt_openize_1a(self,c);
#endif
#ifdef CS_2a
  if(c->csid == 0x2a) return crypt_openize_2a(self,c);
#endif
#ifdef CS_3a
  if(c->csid == 0x3a) return crypt_openize_3a(self,c);
#endif

  return NULL;
}

crypt_t crypt_deopenize(crypt_t self, packet_t open)
{
  crypt_t ret = NULL;
  if(!open) return NULL;
  if(!self) return (crypt_t)packet_free(open);

#ifdef CS_1a
  if((ret = crypt_deopenize_1a(self,open))) return ret;
#endif
#ifdef CS_2a
  if((ret = crypt_deopenize_2a(self,open))) return ret;
#endif
#ifdef CS_3a
  if((ret = crypt_deopenize_3a(self,open))) return ret;
#endif

  if(!ret) return (crypt_t)packet_free(open);
}

crypt_t crypt_merge(crypt_t a, crypt_t b)
{
  if(a)
  {
    if(b)
    {
      // just copy in the deopenized variables
      a->atIn = b->atIn;
      memcpy(a->lineIn, b->lineIn, 16);
#ifdef CS_1a
      if(a->csid == 0x1a) crypt_merge_1a(a,b);
#endif
#ifdef CS_2a
      if(a->csid == 0x2a) crypt_merge_2a(a,b);
#endif
#ifdef CS_3a
      if(a->csid == 0x3a) crypt_merge_3a(a,b);
#endif
      crypt_free(b);      
    }
  }else{
    a = b;
  }
  crypt_lineinit(a);
  return a;
}
