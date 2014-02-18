#ifndef crypt_libtom_h
#define crypt_libtom_h

#include <tomcrypt.h>
#include <tommath.h>
#include <time.h>
#include "crypt.h"

prng_state _crypt_libtom_prng;
int _crypt_libtom_inited = 0;
int _crypt_libtom_err = 0;

int crypt_libtom_init();
char *crypt_libtom_err();

#endif