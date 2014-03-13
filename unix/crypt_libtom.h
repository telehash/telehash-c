#ifndef crypt_libtom_h
#define crypt_libtom_h

#include <tomcrypt.h>
#include <tommath.h>
#include <time.h>
#include "crypt.h"
#include "util.h"

prng_state _crypt_libtom_prng;
int _crypt_libtom_err;

int crypt_libtom_init();

#endif