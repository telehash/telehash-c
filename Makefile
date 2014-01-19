DOTC = src/hn.c src/packet.c src/path.c src/switch.c src/bucket.c src/chan.c src/util.c src/crypt_libtom.c src/xht.c
DOTH = src/hn.h src/packet.h src/path.h src/switch.h src/bucket.h src/chan.h src/util.h src/crypt.h src/xht.h
JS0N = ../js0n/js0n.c ../js0n/j0g.c -I../js0n
LTOM = -ltomcrypt -ltommath -DLTM_DESC
FLAG = -I./src -I./ext

all: test

test: test_*.c $(DOTC) $(DOTH)
	gcc -w -o test_crypt test_crypt.c $(LTOM) $(FLAG)
	gcc -Wall -g -o test_ping test_ping.c ext/seek.c $(DOTC) $(JS0N) $(LTOM) $(FLAG)
	gcc -Wall -g -o test_misc test_misc.c $(DOTC) $(JS0N) $(LTOM) $(FLAG)
	
util: idgen.c openize.c $(DOTC) $(DOTH)
	gcc -Wall -o idgen idgen.c $(LTOM)
	gcc -Wall -g -o openize openize.c $(LTOM) $(JS0N)
	
clean:
	rm -f test_crypt test_ping test_misc idgen openize
	
