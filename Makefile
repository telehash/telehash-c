CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
INCLUDE+=-Iinclude -Iinclude/lib -Iunix

LIB = src/lib/util.c src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chunks.c src/lib/chacha.c src/lib/uri.c
E3X = src/e3x/e3x.c src/e3x/channel3.c src/e3x/self3.c src/e3x/exchange3.c src/e3x/event3.c src/e3x/cipher3.c
MESH = src/mesh.c src/link.c src/links.c src/pipe.c
EXT = src/ext/link.c src/ext/block.c

CS1a = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/aes128.c src/e3x/cs1a/cs1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a src/e3x/cs2a/crypt_libtom_*.c
CS3a = -Ics1a -lsodium -DCS_3a src/e3x/cs3a/crypt_3a.c

# this is CS1a only
UNIX1a = unix/platform.c src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c

# this is CS3a only
#ARCH = -DNOCS_1a unix/platform.c cs3a/crypt_base.c cs1a/base64*.c $(JSON) $(CS3a) $(INCLUDE) $(LIBS)

# CS1a and CS2a
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(INCLUDE) $(LIBS)

# CS1a and CS3a
#ARCH = unix/platform.c cs3a/crypt_base.c $(JSON) $(CS1a) $(CS3a) $(INCLUDE) $(LIBS)

# all
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(CS3a) $(INCLUDE) $(LIBS)
ARCH = $(UNIX1a)

TESTS = lib_base32 lib_lob lib_hashname lib_murmur lib_chunks lib_util e3x_core e3x_cs1a e3x_self3 e3x_exchange3 e3x_event3 e3x_channel3 mesh_core net_loopback net_udp4 net_tcp4 ext_link lib_chacha ext_block lib_uri

LIB_OBJFILES = $(patsubst %.c,%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,%.o,$(EXT))

CORE_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES)

CS1a_OBJFILES = $(patsubst %.c,%.o,$(CS1a))

ARCH_OBJFILES = $(patsubst %.c,%.o,$(ARCH))

FULL_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES)

IDGEN_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES) util/idgen.o
ROUTER_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES) unix/util.o util/router.o src/net/tcp4.o src/net/udp4.o

HEADERS=$(wildcard include/*.h)

#all: libmesh libe3x idgen router
all: idgen router static

# TODO make these lib builds real

static: libe3x libtelehash

libe3x: $(E3X_OBJFILES) $(LIB_OBJFILES)
	rm -f libe3x.a
	ar crs libe3x.a $(E3X_OBJFILES) $(LIB_OBJFILES)

libtelehash: $(FULL_OBJFILES)
	rm -f libtelehash.a
	ar crs libtelehash.a $(FULL_OBJFILES)

.PHONY: arduino test

arduino: 
	mkdir -p arduino/src/telehash
	cp src/*.c src/*.h arduino/src/telehash/
	mkdir -p arduino/src/telehash/lib
	cp src/lib/*.c src/lib/*.h arduino/src/telehash/lib/
	mkdir -p arduino/src/telehash/e3x
	cp src/e3x/*.c src/e3x/*.h arduino/src/telehash/e3x/

test: $(FULL_OBJFILES)
	cd test; $(MAKE) $(MFLAGS)
	
# my make zen is not high enough right now, is yours?

%.o : %.c $(HEADERS)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

idgen: $(IDGEN_OBJFILES)
	$(CC) $(CFLAGS) -o bin/idgen $(IDGEN_OBJFILES)

ping:
	$(CC) $(CFLAGS) -o bin/ping util/ping.c src/*.c unix/util.c $(ARCH)

router: $(ROUTER_OBJFILES)
	$(CC) $(CFLAGS) -o bin/router $(ROUTER_OBJFILES)

mesh:
	$(CC) $(CFLAGS) -o bin/mesh util/mesh.c src/*.c unix/util.c src/ext/*.c $(ARCH)

port:
	$(CC) $(CFLAGS) -o bin/port util/port.c src/*.c unix/util.c src/ext/*.c $(ARCH)
 
clean:
	rm -rf bin/*
	rm -rf arduino/src/telehash/
	rm -f id.json
	cd test; $(MAKE) clean
	find . -name "*.o" -exec rm -f {} \;
