#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct xhashname_struct
{
    char flag;
    struct xhashname_struct *next;
    const char *key;
    void *val;
} *xhn;

struct xht_struct
{
    uint32_t prime;
    uint32_t iter;
    xhn zen;
};

/* Generates a hash code for a string.
 * This function uses the ELF hashing algorithm as reprinted in 
 * Andrew Binstock, "Hashing Rehashed," Dr. Dobb's Journal, April 1996.
 */
uint32_t _xhter(const char *s)
{
    /* ELF hash uses unsigned chars and unsigned arithmetic for portability */
    const unsigned char *name = (const unsigned char *)s;
    uint32_t h = 0, g;

    while (*name)
    {
       /* do some fancy bitwanking on the string */
        h = (h << 4) + (uint32_t)(*name++);
        if ((g = (h & 0xF0000000UL))!=0)
            h ^= (g >> 24);
        h &= ~g;
    }

    return h;
}


xhn _xht_node_find(xhn n, const char *key)
{
    for(;n != 0; n = n->next)
        if(n->key != 0 && strcmp(key, n->key) == 0)
            return n;
    return 0;
}


xht_t xht_new(unsigned int prime)
{
    xht_t xnew;

    xnew = (xht_t)malloc(sizeof(struct xht_struct));
    if(!xnew) return NULL;
    memset(xnew,0,sizeof(struct xht_struct));
    xnew->prime = (uint32_t)prime;
    xnew->zen = (xhn)malloc(sizeof(struct xhashname_struct)*prime); /* array of xhn size of prime */
    if(!xnew->zen)
    {
      free(xnew);
      return NULL;
    }
    memset(xnew->zen,0,sizeof(struct xhashname_struct)*prime);
    return xnew;
}

/* does the set work, used by xht_set and xht_store */
void _xht_set(xht_t h, const char *key, void *val, char flag)
{
    uint32_t i;
    xhn n;

    /* get our index for this key */
    i = _xhter(key) % h->prime;

    /* check for existing key first, or find an empty one */
    if((n = _xht_node_find(&h->zen[i], key)) == 0)
        for(n = &h->zen[i]; n != 0; n = n->next)
            if(n->val == 0)
                break;

    /* if none, make a new one, link into this index */
    if(n == 0)
    {
        n = (xhn)malloc(sizeof(struct xhashname_struct));
        if(!n) return;
        memset(n,0,sizeof(struct xhashname_struct));
        n->next = h->zen[i].next;
        h->zen[i].next = n;
    }

    /* when flag is set, we manage their mem and free em first */
    if(n->flag)
    {
        free((void*)n->key);
        free(n->val);
    }

    n->flag = flag;
    n->key = key;
    n->val = val;
}

void xht_set(xht_t h, const char *key, void *val)
{
    if(h == 0 || key == 0) return;
    _xht_set(h, key, val, 0);
}

void xht_store(xht_t h, const char *key, void *val, size_t vlen)
{
    char *ckey, *cval;
    size_t klen;

    if(h == 0 || key == 0 || (klen = strlen(key)) == 0) return;

    ckey = (char*)malloc(klen+1);
    if(!ckey) return;
    memcpy(ckey,key,klen);
    ckey[klen] = '\0';
    cval = (void*)malloc(vlen);
    if(!cval)
    {
      free(ckey);
      return;
    }
    memcpy(cval,val,vlen);
    _xht_set(h, ckey, cval, 1);
}


void *xht_get(xht_t h, const char *key)
{
    xhn n;

    if(h == 0 || key == 0) return 0;
    if((n = _xht_node_find(&h->zen[_xhter(key) % h->prime], key)) == 0) return 0;

    return n->val;
}


void xht_free(xht_t h)
{
    xhn n, f;
    uint32_t i;

    if(h == 0) return;

    for(i = 0; i < h->prime; i++)
        for(n = (&h->zen[i])->next; n != 0;)
        {
            f = n->next;
            if(n->flag)
            {
                free((void*)n->key);
                free(n->val);
            }
            free(n);
            n = f;
        }

    free(h->zen);
    free(h);
}

void xht_walk(xht_t h, xht_walker w, void *arg)
{
    uint32_t i;
    xhn n;

    if(h == 0 || w == 0)
        return;

    for(i = 0; i < h->prime; i++)
        for(n = &h->zen[i]; n != 0; n = n->next)
            if(n->key != 0 && n->val != 0)
                (*w)(h, n->key, n->val, arg);
}

char *xht_iter(xht_t h, char *key)
{
  xhn n;
  const char *ret;
  if(!h) return NULL;

  // reset/start
  if(!key) h->iter = 0;

  // step through each
  for(ret = NULL;!ret && h->iter < h->prime; h->iter++)
  {
    // find given key in current iter
    for(n = &h->zen[h->iter]; !ret && n; n = n->next)
    {
      // take the first one
      if(!key) ret = n->key;
      else if(n->key == key) key = NULL; // take the next one
      
    }
    if(ret) break;
    // return the next avail key
    key = NULL;
  }
  
  return (char*)ret;
}

