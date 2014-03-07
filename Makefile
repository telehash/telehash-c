SOURCES:=$(shell find lib -type f)
JS0N = ../js0n/js0n.c ../js0n/j0g.c -I../js0n
LTOM = unix/crypt_libtom*.c -ltomcrypt -ltommath -DLTM_DESC -DCS_2a
FLAG = -I./lib -I./ext

all: test apps

apps: $(SOURCES)
	gcc -Wall -g -o seed/seed seed/*.c lib/*.c ext/*.c unix/platform.c $(JS0N) $(LTOM) $(FLAG)	
	gcc -Wall -g -o util/idgen util/idgen.c lib/crypt*.c lib/packet.c lib/util.c unix/platform.c $(JS0N) $(LTOM) $(FLAG)
	gcc -Wall -g -o test/ping test/ping.c lib/*.c ext/*.c unix/platform.c $(JS0N) $(LTOM) $(FLAG)

test: $(SOURCES)
#	gcc -w -o test/crypt test/crypt.c $(LTOM) $(FLAG)
#	gcc -Wall -g -o test/misc test/misc.c lib/*.c $(JS0N) $(LTOM) $(FLAG)

clean:
	rm -f test/crypt test/ping test/misc util/idgen seed/seed
	
