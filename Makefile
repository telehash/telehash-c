DOTC = hn.c packet.c path.c switch.c dht.c util.c crypt_libtom.c xht.c
DOTH = hn.h packet.h path.h switch.h dht.h util.h crypt.h xht.h
JS0N = ../js0n/js0n.c ../js0n/j0g.c -I../js0n
LTOM = -ltomcrypt -ltommath -DLTM_DESC

all: test

test: test_*.c $(DOTC) $(DOTH)
	gcc -w -o test_crypt test_crypt.c $(LTOM)
	gcc -Wall -g -o test_ping test_ping.c $(DOTC) $(JS0N) $(LTOM)
	
util: idgen.c openize.c $(DOTC) $(DOTH)
	gcc -Wall -o idgen idgen.c $(LTOM)
	gcc -Wall -g -o openize openize.c $(LTOM) $(JS0N)
	
clean:
	rm -f test_crypt test_ping idgen openize
	
