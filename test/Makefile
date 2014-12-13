TESTS = lib_base32 lib_lob lib_hashname lib_murmur lib_chunks lib_util \
		e3x_core e3x_cs1a e3x_self3 e3x_exchange3 e3x_event3 e3x_channel3 \
		mesh_core net_loopback net_udp4 net_tcp4 ext_link lib_chacha \
		ext_block lib_uri

CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
INCLUDE+=-I../unix -I../include -I../include/lib

LIB = src/lib/util.c src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chunks.c src/lib/chacha.c src/lib/uri.c
E3X = src/e3x/e3x.c src/e3x/channel3.c src/e3x/self3.c src/e3x/exchange3.c src/e3x/event3.c src/e3x/cipher3.c
MESH = src/mesh.c src/link.c src/links.c src/pipe.c
EXT = src/ext/link.c src/ext/block.c

CS1a = src/e3x/cs1a/aes.c src/e3x/cs1a/hmac.c src/e3x/cs1a/aes128.c src/e3x/cs1a/cs1a.c src/e3x/cs1a/uECC.c src/e3x/cs1a/sha256.c
CS2a = -ltomcrypt -ltommath -DLTM_DESC -DCS_2a src/e3x/cs2a/crypt_libtom_*.c
CS3a = -Ics1a -lsodium -DCS_3a src/e3x/cs3a/crypt_3a.c

# this is CS1a only
UNIX1a = unix/util_sys.c src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c

# this is CS3a only
#ARCH = -DNOCS_1a unix/platform.c cs3a/crypt_base.c cs1a/base64*.c $(JSON) $(CS3a) $(INCLUDE) $(LIBS)

# CS1a and CS2a
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(INCLUDE) $(LIBS)

# CS1a and CS3a
#ARCH = unix/platform.c cs3a/crypt_base.c $(JSON) $(CS1a) $(CS3a) $(INCLUDE) $(LIBS)

# all
#ARCH = unix/platform.c $(JSON) $(CS1a) $(CS2a) $(CS3a) $(INCLUDE) $(LIBS)
ARCH = $(UNIX1a)



LIB_OBJFILES = $(patsubst %.c,../%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,../%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,../%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,../%.o,$(EXT))

CORE_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES)

CS1a_OBJFILES = $(patsubst %.c,../%.o,$(CS1a))

ARCH_OBJFILES = $(patsubst %.c,../%.o,$(ARCH))

OTHER_TEST_FILES=../src/lib/murmur.c ../unix/net_loopback.c ../unix/net_udp4.c ../unix/net_tcp4.c
OTHER_TEST_OBJFILES=$(patsubst %.c,%.o,$(OTHER_TEST_FILES))

FULL_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES) $(OTHER_TEST_FILES)

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
	$(CC) $(INCLUDE) $(CFLAGS) $(patsubst bin/test_%,%.o,$@) -o $@ $(FULL_OBJFILES)

%.o : %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

clean:
	rm -rf bin/*
	rm -f id.json
	rm *.o
