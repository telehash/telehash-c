CC=gcc
EMCC=emcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -DDEBUG
#CFLAGS+=-Weverything -Wno-unused-macros -Wno-undef -Wno-gnu-zero-variadic-macro-arguments -Wno-padded -Wno-gnu-label-as-value -Wno-gnu-designator -Wno-missing-prototypes -Wno-format-nonliteral
INCLUDE+=-Iinclude -Iinclude/lib -Iunix

LIB = src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chacha.c src/lib/murmur.c src/lib/jwt.c src/lib/base64.c src/lib/aes128.c src/lib/sha256.c src/lib/uECC.c
E3X = src/e3x/e3x.c src/e3x/self.c src/e3x/exchange.c src/e3x/cipher.c
MESH = src/mesh.c src/link.c src/chan.c src/gossip.c
EXT = 
#NET = src/net/loopback.c src/net/udp4.c src/net/tcp4.c src/net/serial.c
NET = src/net/loopback.c 
UTIL = src/util/util.c src/util/chunks.c src/util/frames.c src/unix/util.c src/unix/util_sys.c
TMESH = src/tmesh/tmesh.c 

# CS1c by default
CS = src/e3x/cs1c/cs1c.c 

# also CS1a
CS += src/e3x/cs1a/cs1a.c

# check for CS2a deps
ifneq ("$(wildcard node_modules/libtomcrypt-c/libtomcrypt.a)","")
CS += src/e3x/cs2a/cs2a_tom.c
CFLAGS += -DLTM_DESC
LDFLAGS += node_modules/libtomcrypt-c/libtomcrypt.a node_modules/libtommath-c/libtommath.a
INCLUDE += -I./node_modules/libtomcrypt-c/src/headers -I./node_modules/libtommath-c
else
CS += src/e3x/cs2a_disabled.c
endif

# check for CS3a deps
ifneq ("$(wildcard node_modules/libsodium-c/src/libsodium/.libs/libsodium.a)","")
CS += src/e3x/cs3a/cs3a.c
LDFLAGS += node_modules/libsodium-c/src/libsodium/.libs/libsodium.a
INCLUDE += -I./node_modules/libsodium-c/src/libsodium/include
else
CS += src/e3x/cs3a_disabled.c
endif

LIB_OBJFILES = $(patsubst %.c,%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,%.o,$(EXT))
NET_OBJFILES = $(patsubst %.c,%.o,$(NET))
UTIL_OBJFILES = $(patsubst %.c,%.o,$(UTIL))
CS_OBJFILES = $(patsubst %.c,%.o,$(CS))

FULL_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES) $(NET_OBJFILES) $(UTIL_OBJFILES) $(CS_OBJFILES)

IDGEN_OBJFILES = $(FULL_OBJFILES) util/idgen.o
ROUTER_OBJFILES = $(FULL_OBJFILES) src/net/udp4.o util/router.o 
PING_OBJFILES = $(FULL_OBJFILES) util/ping.o 

HEADERS=$(wildcard include/*.h)

all: idgen router ping static
	@echo "TODO\t`git grep TODO | wc -l | tr -d ' '`"

deps:
	npm install

# todo, this header mangling process is a drag

static: libtelehash
	@cat $(LIB) $(E3X) $(MESH) $(EXT) $(UTIL) > telehash.c
	@cat include/lob.h include/xht.h include/e3x_cipher.h include/e3x_self.h include/e3x_exchange.h include/hashname.h include/mesh.h include/link.h include/chan.h include/util_chunks.h include/util_frames.h include/*.h > telehash.h
	@sed -i.bak "/#include \".*h\"/d" telehash.h
	@rm -f telehash.h.bak

static-cs1a:
	@echo "#include <telehash.h>" > telehash.c
	@cat $(LIB) $(E3X) $(MESH) $(EXT) $(UTIL) src/e3x/cs1a/cs1a.c src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c >> telehash.c
	@sed -i '' "/#include \".*h\"/d" telehash.c
	@cat include/lob.h include/xht.h include/e3x_cipher.h include/e3x_self.h include/e3x_exchange.h include/hashname.h include/mesh.h include/link.h include/chan.h include/util_chunks.h include/util_frames.h include/*.h > telehash.h
	@sed -i.bak "/#include \".*h\"/d" telehash.h
	@rm -f telehash.h.bak

static-tmesh:
	@echo "#include <telehash.h>" > telehash.c
	@cat $(LIB) $(E3X) $(MESH) $(EXT) $(UTIL) $(TMESH) >> telehash.c
	@sed -i '' "/#include \".*h\"/d" telehash.c
	@cat include/lob.h include/xht.h include/e3x_cipher.h include/e3x_self.h include/e3x_exchange.h include/hashname.h include/mesh.h include/link.h include/chan.h include/util_chunks.h include/util_frames.h include/*.h > telehash.h
	@sed -i.bak "/#include \".*h\"/d" telehash.h
	@rm -f telehash.h.bak

libtelehash: $(FULL_OBJFILES)
	rm -f libtelehash.a
	ar crs libtelehash.a $(FULL_OBJFILES)

throwback-update:
	cd ../throwback && make static
	cp ../throwback/dew.c throwback/
	cp ../throwback/dew.h throwback/

throwback-test: $(FULL_OBJFILES) throwback/dew.o
	$(CC) $(CFLAGS) -I include/ -o test/bin/test_throwback throwback/test.c throwback/all.c throwback/dew.o $(FULL_OBJFILES) $(LDFLAGS)
	./test/bin/test_throwback

.PHONY: arduino test TAGS

arduino: static
	cp telehash.c arduino/src/telehash/
	@cat src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c >> arduino/src/telehash/telehash.c
	cp src/e3x/cs1a/cs1a.c arduino/src/cs1a/
	cp $(HEADERS) arduino/src/telehash/

test: $(FULL_OBJFILES) ping
	cd test; $(MAKE) $(MFLAGS)

TAGS:
	find . | grep ".*\.\(h\|c\)" | xargs etags -f TAGS

%.o : %.c $(HEADERS)
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

idgen: $(IDGEN_OBJFILES)
	$(CC) $(CFLAGS) -o bin/idgen $(IDGEN_OBJFILES) $(LDFLAGS)

ping:
#ping: $(PING_OBJFILES)
#	$(CC) $(CFLAGS) -o bin/ping $(PING_OBJFILES) $(LDFLAGS)

router: $(ROUTER_OBJFILES)
	$(CC) $(CFLAGS) -o bin/router $(ROUTER_OBJFILES) $(LDFLAGS)

#mesh:
#	$(CC) $(CFLAGS) -o bin/mesh util/mesh.c src/*.c unix/util.c src/ext/*.c $(ARCH)

#port:
#	$(CC) $(CFLAGS) -o bin/port util/port.c src/*.c unix/util.c src/ext/*.c $(ARCH)

clean:
	rm -rf bin/*
	rm -f arduino/src/telehash/*.h
	rm -f arduino/src/telehash/*.c
	rm -f id.json
	cd test; $(MAKE) clean
	find . -name "*.o" -exec rm -f {} \;
	rm -f lib*.a
