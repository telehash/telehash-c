SOURCES:=$(shell find . -type f)
JS0N = ../js0n/js0n.c ../js0n/j0g.c -I../js0n
LTOM = -ltomcrypt -ltommath -DLTM_DESC
FLAG = -I./src -I./ext

all: test apps

apps: $(SOURCES)
	gcc -Wall -g -o seed/seed seed/*.c src/*.c ext/*.c $(JS0N) $(LTOM) $(FLAG)	
	gcc -Wall -o util/idgen util/idgen.c $(LTOM)
	gcc -Wall -g -o util/openize util/openize.c $(LTOM) $(JS0N)

test: $(SOURCES)
	gcc -w -o test/crypt test/crypt.c $(LTOM) $(FLAG)
	gcc -Wall -g -o test/ping test/ping.c src/*.c ext/*.c $(JS0N) $(LTOM) $(FLAG)
	gcc -Wall -g -o test/misc test/misc.c src/*.c $(JS0N) $(LTOM) $(FLAG)

clean:
	rm -f test/crypt test/ping test/misc util/idgen util/openize seed/seed
	
