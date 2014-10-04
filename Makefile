CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter
INCLUDE+=-Iunix -Isrc -Isrc/lib -Isrc/ext -Isrc/e3x

LIB = src/lib/util.c src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/j0g.c
E3X = src/e3x/e3x.c src/e3x/chan3.c src/e3x/self3.c src/e3x/ex3.c src/e3x/ev3.c

CS1a = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/aes128.c src/e3x/cs1a/base64_dec.c src/e3x/cs1a/crypt_1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c src/e3x/cs1a/base64_enc.c
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a src/e3x/cs2a/crypt_libtom_*.c
CS3a = -Ics1a -lsodium -DCS_3a src/e3x/cs3a/crypt_3a.c

# this is CS1a only
ARCH = unix/platform.c src/e3x/cs1a/crypt_base.c $(LIB) $(E3X) $(CS1a) $(INCLUDE) $(LIBS)

# this is CS3a only
#ARCH = -DNOCS_1a unix/platform.c cs3a/crypt_base.c cs1a/base64*.c $(JSON) $(CS3a) $(INCLUDE) $(LIBS)

# CS1a and CS2a
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(INCLUDE) $(LIBS)

# CS1a and CS3a
#ARCH = unix/platform.c cs3a/crypt_base.c $(JSON) $(CS1a) $(CS3a) $(INCLUDE) $(LIBS)

# all
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(CS3a) $(INCLUDE) $(LIBS)


LIBS+=
all: idgen

# TODO, create a static libe3x.a build option

test:
	$(CC) $(CFLAGS) -o bin/test util/test.c $(ARCH)

idgen:
	$(CC) $(CFLAGS) -o bin/idgen util/idgen.c $(ARCH)

ping:
	$(CC) $(CFLAGS) -o bin/ping util/ping.c src/*.c unix/util.c $(ARCH)

router:
	$(CC) $(CFLAGS) -o bin/router util/router.c src/*.c unix/util.c src/ext/*.c $(ARCH)

mesh:
	$(CC) $(CFLAGS) -o bin/mesh util/mesh.c src/*.c unix/util.c src/ext/*.c $(ARCH)

port:
	$(CC) $(CFLAGS) -o bin/port util/port.c src/*.c unix/util.c src/ext/*.c $(ARCH)
 
clean:
	rm -f bin/*
	rm -f id.json
