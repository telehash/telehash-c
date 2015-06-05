TESTS = lib_base32 lib_lob lib_hashname lib_murmur lib_chunks lib_util \
		e3x_core e3x_self e3x_exchange e3x_event e3x_channel \
		mesh_core net_loopback net_udp4 net_tcp4 ext_stream lib_chacha \
		ext_block lib_uri net_handshake lib_socketio lib_jwt lib_base64 \
		ext_path ext_peer net_serial

CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
INCLUDE+=-I../unix -I../include -I../include/lib


LIB = src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chacha.c src/lib/murmur.c src/lib/socketio.c src/lib/jwt.c src/lib/base64.c
E3X = src/e3x/e3x.c src/e3x/channel.c src/e3x/self.c src/e3x/exchange.c src/e3x/event.c src/e3x/cipher.c
MESH = src/mesh.c src/link.c src/pipe.c
EXT = src/ext/stream.c src/ext/block.c src/ext/path.c src/ext/peer.c
NET = src/net/loopback.c src/net/udp4.c src/net/tcp4.c src/net/serial.c
UTIL = src/util/util.c src/util/uri.c src/util/chunks.c src/unix/util.c src/unix/util_sys.c

# CS1a by default
CS = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/cs1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c src/e3x/cs1a/aes-internal-enc.c src/e3x/cs1a/aes-internal.c
TESTS += e3x_cs1a

# check for CS2a deps
ifneq ("$(wildcard ../node_modules/libtomcrypt-c/libtomcrypt.a)","")
CS += src/e3x/cs2a/cs2a_tom.c
CFLAGS += -DLTM_DESC
LDFLAGS += ../node_modules/libtomcrypt-c/libtomcrypt.a ../node_modules/libtommath-c/libtommath.a
INCLUDE += -I../node_modules/libtomcrypt-c/src/headers -I../node_modules/libtommath-c
TESTS += e3x_cs2a
else
CS += src/e3x/cs2a_disabled.c
endif

# check for CS3a deps
ifneq ("$(wildcard ../node_modules/libsodium-c/src/libsodium/.libs/libsodium.a)","")
CS += src/e3x/cs3a/cs3a.c
LDFLAGS += ../node_modules/libsodium-c/src/libsodium/.libs/libsodium.a
INCLUDE += -I../node_modules/libsodium-c/src/libsodium/include
TESTS += e3x_cs3a
else
CS += src/e3x/cs3a_disabled.c
endif


LIB_OBJFILES = $(patsubst %.c,../%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,../%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,../%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,../%.o,$(EXT))
NET_OBJFILES = $(patsubst %.c,../%.o,$(NET))
UTIL_OBJFILES = $(patsubst %.c,../%.o,$(UTIL))
CS_OBJFILES = $(patsubst %.c,../%.o,$(CS))

FULL_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES) $(NET_OBJFILES) $(UTIL_OBJFILES) $(CS_OBJFILES)

all: test

test: test-slink tests test-interop

test-slink: net_slink.o bin/test_net_slink

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
	$(CC) $(INCLUDE) $(CFLAGS) -o $@ $(patsubst bin/test_%,%.o,$@) $(FULL_OBJFILES) $(LDFLAGS) 

%.o : %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	rm -rf bin/*
	rm -f id.json
	rm -f *.o
