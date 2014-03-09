CC=gcc
CFLAGS+=-fsigned-char 
INCLUDE+=-I../lib -I../ext -I. -I../../js0n
LIBS+=-lrt
original:
	 $(CC) $(CFLAGS) -o original_test original_test.c aes.c ecc.c hmac.c sha1.c sha256.c platform.c $(INCLUDE) $(LIBS)
test:
	$(CC) $(CFLAGS) -o test test.c ../../js0n/js0n.c ../../js0n/j0g.c ../lib/packet.c ../lib/util.c base64_dec.c base64_enc.c crypt.c crypt_1a.c aes.c ecc.c hmac.c sha1.c sha256.c platform.c $(INCLUDE) $(LIBS)
idgen:
	$(CC) $(CFLAGS) -o idgen idgen.c ../../js0n/js0n.c ../../js0n/j0g.c ../lib/packet.c ../lib/util.c base64_dec.c base64_enc.c crypt.c crypt_1a.c  aes.c ecc.c hmac.c sha1.c sha256.c platform.c $(INCLUDE) $(LIBS)
#	$(CC) $(CFLAGS) -o ping ping.c ../../js0n/js0n.c ../../js0n/j0g.c ../lib/*.c base64_dec.c base64_enc.c crypt.c crypt_1a.c aes.c ecc.c hmac.c sha1.c sha256.c platform.c $(INCLUDE) $(LIBS) 
clean:
	rm -f *test
	rm -f idgen
	rm id.json
