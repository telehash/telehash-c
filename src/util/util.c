#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "telehash.h"

char *util_hex(uint8_t *in, size_t len, char *out)
{
    uint32_t j;
    char *c = out;
    static char *hex = "0123456789abcdef";
    static char *buf = NULL;
    if(!in || !len) return NULL;

    // utility mode only! use/return an internal buffer
    if(!out && !(c = out = buf = realloc(buf,len*2+1))) return NULL;

    for (j = 0; j < len; j++) {
      *c = hex[((in[j]&240)/16)];
      c++;
      *c = hex[in[j]&15];
      c++;
    }
    *c = '\0';
    return out;
}

char hexcode(char x)
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

uint8_t *util_unhex(char *in, size_t len, uint8_t *out)
{
  uint32_t j;
  uint8_t *c = out;
  if(!out || !in) return NULL;
  if(!len) len = strlen(in);

  for(j=0; (j+1)<len; j+=2)
  {
    *c = ((hexcode(in[j]) * 16) & 0xF0) + (hexcode(in[j+1]) & 0xF);
    c++;
  }
  return out;
}

char *util_ishex(char *str, uint32_t len)
{
  uint32_t i;
  for(i=0;i<len;i++)
  {
    if(!str[i]) return NULL;
    if(str[i] == hexcode(str[i])) return NULL;
  }
  return str;
}

int util_cmp(char *a, char *b)
{
  if(!a || !b) return -1;
  if(a == b) return 0;
  if(strlen(a) != strlen(b)) return -1;
  return util_ct_memcmp(a,b,strlen(a));
}

// default alpha sort
int _util_sort_alpha(void *arg, const void *a, const void *b)
{
  char *aa = *(char**)a;
  char *bb = *(char**)b;
  if(!aa) return -1;
  if(!bb) return 1;
  return strcmp(aa,bb);
}

// from http://git.uclibc.org/uClibc/tree/libc/stdlib/stdlib.c?id=515d54433138596e81267237542bd9168b8cc787#n789
/* This code is derived from a public domain shell sort routine by
 * Ray Gardner and found in Bob Stout's snippets collection. */

void util_sort(void *base, unsigned int nel, unsigned int width, int (*comp)(void *, const void *, const void *), void *arg)
{
  unsigned int wgap, i, j, k;
  char tmp;

  // added this for default alpha sort
  if(!comp) comp = _util_sort_alpha;

  if ((nel > 1) && (width > 0)) {
    wgap = 0;
    do {
      wgap = 3 * wgap + 1;
    } while (wgap < (nel-1)/3);
    /* From the above, we know that either wgap == 1 < nel or */
    /* ((wgap-1)/3 < (int) ((nel-1)/3) <= (nel-1)/3 ==> wgap <  nel. */
    wgap *= width;			/* So this can not overflow if wnel doesn't. */
    nel *= width;			/* Convert nel to 'wnel' */
    do {
      i = wgap;
      do {
        j = i;
        do {
          register char *a;
          register char *b;

          j -= wgap;
          a = j + ((char *)base);
          b = a + wgap;
          if ((*comp)(arg, a, b) <= 0) {
            break;
          }
          k = width;
          do {
            tmp = *a;
            *a++ = *b;
            *b++ = tmp;
          } while (--k);
        } while (j >= wgap);
        i += width;
      } while (i < nel);
      wgap = (wgap - width)/3;
    } while (wgap);
  }
}

// portable friendly reallocf
void *util_reallocf(void *ptr, size_t size)
{
  void *ra;
  // zero == free
  if(!size)
  {
    if(ptr) free(ptr);
    return NULL;
  }
  ra = realloc(ptr,size);
  if(ra) return ra;
  free(ptr);
  return NULL;
}

uint64_t util_at(void)
{
  uint64_t at;
  uint32_t *half = (uint32_t*)(&at);

  // store both current seconds and ms since then in one value
  half[0] = util_sys_seconds();
  half[1] = (uint32_t)util_sys_ms(half[0]);

  return at;
}

uint32_t util_since(uint64_t at)
{
  uint32_t *half = (uint32_t*)(&at);
  return ((uint32_t)util_sys_ms(half[0]) - half[1]);
}

int util_ct_memcmp(const void* s1, const void* s2, size_t n)
{
    const unsigned char *p1 = s1, *p2 = s2;
    int x = 0;

    while (n--)
    {
        x |= (*p1 ^ *p2);
        p1++;
        p2++;
    }

    /* Don't leak any info besides failure */
    if (x)
        x = 1;

    return x;
}

// embedded may not have strdup but it's a kinda handy shortcut
char *util_strdup(const char *str)
{
  char *ret;
  size_t len = 0;
  if(str) len = strlen(str);
  if(!(ret = malloc(len+1))) return NULL;
  memcpy(ret,str,len);
  ret[len] = 0;
  return ret;
}

