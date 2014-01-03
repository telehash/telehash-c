DOTC = hn.c packet.c util.c crypt_libtom.c xht.c
DOTH = hn.h packet.h util.h crypt.h xht.h
JS0N = ../js0n/js0n.c ../js0n/j0g.c -I../js0n
LTOM = -ltomcrypt -ltommath -DLTM_DESC

all: test util

test: test_*.c $(DOTC) $(DOTH)
	gcc -o test_crypt test_crypt.c $(LTOM)
	gcc -Wall -g -o test_ping test_ping.c $(DOTC) $(JS0N) $(LTOM)
	
util: idgen.c openize.c $(DOTC) $(DOTH)
	gcc -Wall -o idgen idgen.c $(LTOM)
	gcc -Wall -g -o openize openize.c $(LTOM) $(JS0N)
	
clean:
	rm -f test_crypt test_ping idgen openize
	
