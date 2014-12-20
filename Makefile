CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
CFLAGS+=-Weverything -Wno-unused-macros -Wno-undef -Wno-gnu-zero-variadic-macro-arguments -Wno-padded -Wno-gnu-label-as-value -Wno-gnu-designator -Wno-missing-prototypes -Wno-format-nonliteral
INCLUDE+=-Iinclude -Iinclude/lib -Iunix

LIB = src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chacha.c src/lib/murmur.c
E3X = src/e3x/e3x.c src/e3x/channel.c src/e3x/self.c src/e3x/exchange.c src/e3x/event.c src/e3x/cipher.c
MESH = src/mesh.c src/link.c src/pipe.c
EXT = src/ext/link.c src/ext/block.c
NET = src/net/loopback.c src/net/udp4.c src/net/tcp4.c
UTIL = src/util/util.c src/util/uri.c src/util/chunks.c src/unix/util.c src/unix/util_sys.c

CS1a = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/aes128.c src/e3x/cs1a/cs1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a src/e3x/cs2a/crypt_libtom_*.c
CS3a = -Ics1a -lsodium -DCS_3a src/e3x/cs3a/crypt_3a.c

# this is CS1a only
UNIX1a = src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c

# this is CS3a only
#ARCH = -DNOCS_1a unix/platform.c cs3a/crypt_base.c cs1a/base64*.c $(JSON) $(CS3a) $(INCLUDE) $(LIBS)

# CS1a and CS2a
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(INCLUDE) $(LIBS)

# CS1a and CS3a
#ARCH = unix/platform.c cs3a/crypt_base.c $(JSON) $(CS1a) $(CS3a) $(INCLUDE) $(LIBS)

# all
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(CS3a) $(INCLUDE) $(LIBS)
ARCH = $(UNIX1a)

LIB_OBJFILES = $(patsubst %.c,%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,%.o,$(EXT))
NET_OBJFILES = $(patsubst %.c,%.o,$(NET))
UTIL_OBJFILES = $(patsubst %.c,%.o,$(UTIL))

CORE_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES) $(NET_OBJFILES) $(UTIL_OBJFILES)

CS1a_OBJFILES = $(patsubst %.c,%.o,$(CS1a))

ARCH_OBJFILES = $(patsubst %.c,%.o,$(ARCH))

FULL_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES)

IDGEN_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES) util/idgen.o
ROUTER_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES) util/router.o

HEADERS=$(wildcard include/*.h)

all: idgen router static
	@echo "TODO\t`git grep TODO | wc -l | tr -d ' '`"


static: libtelehash
	@cat $(LIB) $(E3X) $(MESH) $(EXT) $(NET) $(UTIL) > telehash.c

libtelehash: $(FULL_OBJFILES)
	rm -f libtelehash.a
	ar crs libtelehash.a $(FULL_OBJFILES)

.PHONY: arduino test

arduino: static
	cp telehash.c arduino/src/telehash/
	cp $(HEADERS) arduino/src/telehash/

test: $(FULL_OBJFILES)
	cd test; $(MAKE) $(MFLAGS)

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
	rm -f arduino/src/telehash/*.h
	rm -f arduino/src/telehash/*.c
	rm -f id.json
	cd test; $(MAKE) clean
	find . -name "*.o" -exec rm -f {} \;
	rm -f lib*.a
