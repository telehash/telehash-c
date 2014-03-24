CC=gcc
CFLAGS+=-g -Wall -DDEBUG
INCLUDE+=-Iunix -Ilib -Iext
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a unix/crypt_libtom*.c
JSON = ../js0n/js0n.c ../js0n/j0g.c -I../js0n
CS1a = -DCS_1a unix/aes.c unix/crypt_base.c unix/sha1.c unix/base64_dec.c unix/crypt_1a.c unix/ecc.c unix/sha256.c unix/base64_enc.c  unix/sha2_small_common.c

#ARCH = unix/platform.c $(JSON) $(CS1a) $(INCLUDE) $(LIBS)
ARCH = unix/platform.c $(JSON) $(CS2a) $(INCLUDE) $(LIBS)

LIBS+=
all: idgen ping seed

test:
	$(CC) $(CFLAGS) -o bin/test util/test.c lib/packet.c lib/crypt.c lib/util.c $(ARCH)

idgen:
	$(CC) $(CFLAGS) -o bin/idgen util/idgen.c lib/packet.c lib/crypt.c lib/util.c $(ARCH)

ping:
	$(CC) $(CFLAGS) -o bin/ping util/ping.c lib/*.c unix/util.c $(ARCH)

seed:
	$(CC) $(CFLAGS) -o bin/seed util/seed.c lib/*.c unix/util.c ext/seek.c ext/path.c $(ARCH)
 
clean:
	rm -f bin/*
	rm -f id.json
