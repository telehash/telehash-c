#include "chacha.h"

// public domain, original source: https://gist.github.com/thoughtpolice/2b36e168d2d7582ad58b

static unsigned int
L32(unsigned int x,unsigned int n)
{
  /* See: http://blog.regehr.org/archives/1063 */
  return (x<<n) | (x>>(-n&31));
}

static unsigned int
ld32(const unsigned char* x)
{
  unsigned int u = x[3];
  u = (u<<8)|x[2];
  u = (u<<8)|x[1];
  return (u<<8)|x[0];
}

static void st32(unsigned char *x,unsigned int u)
{
  int i;
  for (i=0; i < 4; ++i) { x[i] = (0xff & u); u >>= 8; }
}

static void
QR(unsigned int* x,
   unsigned int a,
   unsigned int b,
   unsigned int c,
   unsigned int d)
{
  x[a] = x[a]+x[b]; x[d] = L32(x[d]^x[a],16);
  x[c] = x[c]+x[d]; x[b] = L32(x[b]^x[c],12);
  x[a] = x[a]+x[b]; x[d] = L32(x[d]^x[a], 8);
  x[c] = x[c]+x[d]; x[b] = L32(x[b]^x[c], 7);
}

static void
core(unsigned char o[64], const unsigned int in[16])
{
  unsigned int x[16];
  int i;

  for (i = 0; i < 16; ++i) { x[i] = in[i]; }
  for (i = 0; i < 10; ++i) {
    /* Column round */
    QR(x, 0, 4, 8,  12);
    QR(x, 1, 5, 9,  13);
    QR(x, 2, 6, 10, 14);
    QR(x, 3, 7, 11, 15);
    /* Diagonal round */
    QR(x, 0, 5, 10, 15);
    QR(x, 1, 6, 11, 12);
    QR(x, 2, 7, 8,  13);
    QR(x, 3, 4, 9,  14);
  }

  for (i = 0; i < 16; ++i) { x[i] = x[i] + in[i]; } /* Add input */
  for (i = 0; i < 16; ++i) { st32(o+(4*i), x[i]); } /* Output */
}

void
chacha20_kexp(unsigned int* st,
                            const unsigned char* n,
                            const unsigned char* k)
{
  /* Constants */
  st[0]  = 0x61707865;
  st[1]  = 0x3320646e;
  st[2]  = 0x79622d32;
  st[3]  = 0x6b206574;
  /* 4-11: key bytes */
  st[4]  = ld32(k);
  st[5]  = ld32(k+4);
  st[6]  = ld32(k+8);
  st[7]  = ld32(k+12);
  st[8]  = ld32(k+16);
  st[9]  = ld32(k+20);
  st[10] = ld32(k+24);
  st[11] = ld32(k+28);
  /* Counter + Nonce */
  st[12] = 0;
  st[13] = 0;
  st[14] = ld32(n);
  st[15] = ld32(n+4);
}

int
chacha20_xor(unsigned char* c,
                           const unsigned char* m,
                           unsigned long long b,
                           const unsigned char* n,
                           const unsigned char* k)
{
  unsigned long long i;
  unsigned int st[16];
  unsigned char blk[64];

  chacha20_kexp(st, n, k);

  for(;;) {
    /* Advance state */
    core(blk, st);
    st[12] = st[12]+1;
    if (st[12] == 0) { st[13] = st[13]+1; }
    /* Fast path */
    if (b <= 64) {
      for (i = 0; i < b; ++i) c[i] = m[i] ^ blk[i];
      return 0;
    }
    /* Normal path */
    for (i = 0; i < 64; ++i) { c[i] = m[i] ^ blk[i]; }
    b -= 64;
    m += 64;
    c += 64;
  }
}

int
chacha20_base(unsigned char* o,
                       unsigned long long ol,
                       const unsigned char* n,
                       const unsigned char* k)
{
  unsigned long long i;
  for (i=0; i < ol; ++i) { o[i] = 0; }
  return chacha20_xor(o,o,ol,n,k);
}

uint8_t *chacha20(uint8_t *key, uint8_t *nonce, uint8_t *bytes, uint32_t len)
{
  if(chacha20_xor(bytes,bytes,len,nonce,key)) return 0;
  return bytes;
}
