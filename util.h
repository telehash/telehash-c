#ifndef util_h
#define util_h

#include "packet.h"

packet_t util_file2packet(char *file);
unsigned char *util_hex(unsigned char *in, int len, unsigned char *out);
unsigned char *util_unhex(unsigned char *in, int len, unsigned char *out);

#endif