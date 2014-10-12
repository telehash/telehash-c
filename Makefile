CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
INCLUDE+=-Iunix -Isrc -Isrc/lib -Isrc/ext -Isrc/e3x

LIB = src/lib/util.c src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c
E3X = src/e3x/e3x.c src/e3x/channel3.c src/e3x/self3.c src/e3x/exchange3.c src/e3x/event3.c src/e3x/cipher3.c

CS1a = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/aes128.c src/e3x/cs1a/cs1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a src/e3x/cs2a/crypt_libtom_*.c
CS3a = -Ics1a -lsodium -DCS_3a src/e3x/cs3a/crypt_3a.c

# this is CS1a only
UNIX1a = unix/platform.c src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c  $(LIB) $(E3X) $(CS1a) $(INCLUDE) $(LIBS)

# this is CS3a only
#ARCH = -DNOCS_1a unix/platform.c cs3a/crypt_base.c cs1a/base64*.c $(JSON) $(CS3a) $(INCLUDE) $(LIBS)

# CS1a and CS2a
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(INCLUDE) $(LIBS)

# CS1a and CS3a
#ARCH = unix/platform.c cs3a/crypt_base.c $(JSON) $(CS1a) $(CS3a) $(INCLUDE) $(LIBS)

# all
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(CS3a) $(INCLUDE) $(LIBS)

TESTS = lib_base32 lib_lob lib_hashname lib_murmur lib_util e3x_core e3x_cs1a e3x_self3

all: test

# TODO, create a static libe3x.a build option

test: $(TESTS)
	@for test in $(TESTS); do \
		chmod 0755 ./test/$$test && \
		echo "=====[ running $$test ]=====" && \
		if ./test/$$test ; then \
			rm -f ./test/$$test; \
			echo "PASSED: $$test"; \
		else \
			rm -f ./test/$$test; \
			echo "FAILED: $$test"; exit 1; \
		fi; \
	done

# my make zen is not high enough right now, is yours?

lib_base32:
	$(CC) $(CFLAGS) -o test/lib_base32 test/lib_base32.c src/lib/base32.c $(INCLUDE)

lib_lob:
	$(CC) $(CFLAGS) -o test/lib_lob test/lib_lob.c $(UNIX1a)

lib_hashname:
	$(CC) $(CFLAGS) -o test/lib_hashname test/lib_hashname.c $(UNIX1a)

lib_murmur:
	$(CC) $(CFLAGS) -o test/lib_murmur test/lib_murmur.c src/lib/murmur.c $(INCLUDE)

lib_util:
	$(CC) $(CFLAGS) -o test/lib_util test/lib_util.c src/lib/util.c $(INCLUDE)

e3x_core:
	$(CC) $(CFLAGS) -o test/e3x_core test/e3x_core.c unix/platform.c src/e3x/cs1a_disabled.c src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c $(LIB) $(E3X) $(INCLUDE)

e3x_cs1a:
	$(CC) $(CFLAGS) -o test/e3x_cs1a test/e3x_cs1a.c $(UNIX1a)

e3x_self3:
	$(CC) $(CFLAGS) -o test/e3x_self3 test/e3x_self3.c $(UNIX1a)

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
