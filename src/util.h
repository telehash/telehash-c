#ifndef util_h
#define util_h

#include "packet.h"

packet_t util_file2packet(char *file);
// make sure out is 2*len + 1
unsigned char *util_hex(unsigned char *in, int len, unsigned char *out);
// out must be len/2
unsigned char *util_unhex(unsigned char *in, int len, unsigned char *out);

// safter string comparison (0 == same)
int util_cmp(char *a, char *b);

#endif