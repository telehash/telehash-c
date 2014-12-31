TESTS = lib_base32 lib_lob lib_hashname lib_murmur lib_chunks lib_util \
		e3x_core e3x_self e3x_exchange e3x_event e3x_channel \
		mesh_core net_loopback net_udp4 net_tcp4 ext_stream lib_chacha \
		ext_block lib_uri net_handshake

CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
INCLUDE+=-I../unix -I../include -I../include/lib


LIB = src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chacha.c src/lib/murmur.c
E3X = src/e3x/e3x.c src/e3x/channel.c src/e3x/self.c src/e3x/exchange.c src/e3x/event.c src/e3x/cipher.c
MESH = src/mesh.c src/link.c src/pipe.c
EXT = src/ext/stream.c src/ext/block.c
NET = src/net/loopback.c src/net/udp4.c src/net/tcp4.c
UTIL = src/util/util.c src/util/uri.c src/util/chunks.c src/unix/util.c src/unix/util_sys.c

CS1a = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/aes128.c src/e3x/cs1a/cs1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a src/e3x/cs2a/crypt_libtom_*.c
CS3a = src/e3x/cs3a/cs3a.c

# this is CS1a only
#ARCH = src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c $(CS1a)
#TESTS += e3x_cs1a

# this is CS3a only
ARCH = src/e3x/cs2a_disabled.c src/e3x/cs1a_disabled.c $(CS3a)
LDFLAGS += ../node_modules/libsodium-c/src/libsodium/.libs/libsodium.a
INCLUDE += -I../node_modules/libsodium-c/src/libsodium/include
TESTS += e3x_cs3a

# CS1a and CS2a
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(INCLUDE) $(LIBS)

# CS1a and CS3a
#ARCH = unix/platform.c cs3a/crypt_base.c $(JSON) $(CS1a) $(CS3a) $(INCLUDE) $(LIBS)

# all
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(CS3a) $(INCLUDE) $(LIBS)

LIB_OBJFILES = $(patsubst %.c,../%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,../%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,../%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,../%.o,$(EXT))
NET_OBJFILES = $(patsubst %.c,../%.o,$(NET))
UTIL_OBJFILES = $(patsubst %.c,../%.o,$(UTIL))

CORE_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES) $(NET_OBJFILES) $(UTIL_OBJFILES)

ARCH_OBJFILES = $(patsubst %.c,../%.o,$(ARCH))

FULL_OBJFILES = $(CORE_OBJFILES) $(ARCH_OBJFILES)

all: test

test: tests test-interop

test-interop: net_link.o bin/test_net_link
	@if ./interop.sh ; then \
		echo "PASSED: interop.sh"; \
	else \
		echo "FAILED: interop.sh"; exit 1; \
	fi;

tests: build-tests
	@for test in $(TESTS); do \
		chmod 0755 ./bin/test_$$test && \
		echo "=====[ running $$test ]=====" && \
		if ./bin/test_$$test ; then \
			echo "PASSED: $$test"; \
		else \
			echo "FAILED: $$test"; exit 1; \
		fi; \
	done

build-tests: $(patsubst %,%.o,$(TESTS)) $(patsubst %,bin/test_%,$(TESTS))

bin/test_% : %.o $(FULL_OBJFILES)
	$(CC) $(INCLUDE) $(CFLAGS) $(LDFLAGS) $(patsubst bin/test_%,%.o,$@) -o $@ $(FULL_OBJFILES)

%.o : %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	rm -rf bin/*
	rm -f id.json
	rm -f *.o
