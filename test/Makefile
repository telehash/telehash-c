TESTS = lib_base32 lib_lob lib_hashname lib_murmur lib_chunks lib_util \
		e3x_core e3x_cs1a e3x_self3 e3x_exchange3 e3x_event3 e3x_channel3 \
		mesh_core net_loopback net_udp4 net_tcp4 ext_link lib_chacha \
		ext_block

CC=gcc
CFLAGS+=-g -Wall -Wextra -Wno-unused-parameter -DDEBUG
INCLUDE+=-I../unix -I../src -I../src/lib -I../src/ext -I../src/e3x -I../src/net

LIB = src/lib/util.c src/lib/lob.c src/lib/hashname.c src/lib/xht.c src/lib/js0n.c src/lib/base32.c src/lib/chunks.c src/lib/chacha.c
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



LIB_OBJFILES = $(patsubst %.c,../%.o,$(LIB))
E3X_OBJFILES = $(patsubst %.c,../%.o,$(E3X))
MESH_OBJFILES = $(patsubst %.c,../%.o,$(MESH))
EXT_OBJFILES = $(patsubst %.c,../%.o,$(EXT))

CORE_OBJFILES = $(LIB_OBJFILES) $(E3X_OBJFILES) $(MESH_OBJFILES) $(EXT_OBJFILES)

CS1a_OBJFILES = $(patsubst %.c,../%.o,$(CS1a))

ARCH_OBJFILES = $(patsubst %.c,../%.o,$(ARCH))

FULL_OBJFILES = $(CORE_OBJFILES) $(CS1a_OBJFILES) $(ARCH_OBJFILES)

#all: libmesh libe3x idgen router
all: test

# TODO make these lib builds real

libe3x:
	rm -f libe3x.a
	ar cru libe3x.a unix/platform.c src/e3x/cs2a_disabled.c src/e3x/cs3a_disabled.c  $(LIB) $(E3X) $(CS1a)
	ranlib libe3x.a

libmesh:
	rm -f libmesh.a
	ar cru libmesh.a libe3x.a $(MESH)
	ranlib libmesh.a

.PHONY: arduino

arduino: 
	mkdir -p arduino/src/telehash
	cp src/*.c src/*.h arduino/src/telehash/
	mkdir -p arduino/src/telehash/lib
	cp src/lib/*.c src/lib/*.h arduino/src/telehash/lib/
	mkdir -p arduino/src/telehash/e3x
	cp src/e3x/*.c src/e3x/*.h arduino/src/telehash/e3x/

test-interop: net_link
	@if ./interop.sh ; then \
		echo "PASSED: interop.sh"; \
	else \
		echo "FAILED: interop.sh"; exit 1; \
	fi;

test: $(TESTS) test-interop
	@for test in $(TESTS); do \
		chmod 0755 ./bin/test_$$test && \
		echo "=====[ running $$test ]=====" && \
		if ./bin/test_$$test ; then \
			echo "PASSED: $$test"; \
		else \
			echo "FAILED: $$test"; exit 1; \
		fi; \
	done

# my make zen is not high enough right now, is yours?

%.o : %.c
	$(CC) $(INCLUDE) $(CFLAGS) -c $< -o $@

lib_base32:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_base32 lib_base32.c ../src/lib/base32.c $(INCLUDE)

lib_lob:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_lob lib_lob.c $(FULL_OBJFILES)

lib_hashname:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_hashname lib_hashname.c $(FULL_OBJFILES)

lib_murmur:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_murmur lib_murmur.c ../src/lib/murmur.c $(INCLUDE)

lib_chacha:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_chacha lib_chacha.c ../src/lib/chacha.c ../src/lib/util.c $(INCLUDE)

lib_chunks:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_chunks lib_chunks.c $(FULL_OBJFILES)

#lib_uri:
#	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_uri lib_uri.c ../src/lib/uri.c $(FULL_OBJFILES)

lib_util:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_lib_util lib_util.c ../src/lib/util.c $(INCLUDE)

e3x_core:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_e3x_core e3x_core.c ../unix/platform.c ../src/e3x/cs1a_disabled.c ../src/e3x/cs2a_disabled.c ../src/e3x/cs3a_disabled.c $(LIB_OBJFILES) $(E3X_OBJFILES) $(INCLUDE)

e3x_cs1a:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_e3x_cs1a e3x_cs1a.c $(FULL_OBJFILES)

e3x_self3:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_e3x_self3 e3x_self3.c $(FULL_OBJFILES)

e3x_exchange3:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_e3x_exchange3 e3x_exchange3.c $(FULL_OBJFILES)

e3x_event3:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_e3x_event3 e3x_event3.c $(FULL_OBJFILES)

e3x_channel3:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_e3x_channel3 e3x_channel3.c $(FULL_OBJFILES)

mesh_core:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_mesh_core mesh_core.c $(FULL_OBJFILES)

net_loopback:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_net_loopback net_loopback.c ../src/net/loopback.c $(FULL_OBJFILES)

net_udp4:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_net_udp4 net_udp4.c ../src/net/udp4.c $(FULL_OBJFILES)

net_tcp4:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_net_tcp4 net_tcp4.c ../src/net/tcp4.c ../unix/util.c $(FULL_OBJFILES)

net_link:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_net_link net_link.c ../src/net/udp4.c $(FULL_OBJFILES)

ext_link:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_ext_link ext_link.c ../src/net/loopback.c $(FULL_OBJFILES)

ext_block:
	$(CC) $(CFLAGS) $(INCLUDE) -o bin/test_ext_block ext_block.c ../src/net/loopback.c $(FULL_OBJFILES)

clean:
	rm -rf bin/*
	rm -f id.json
