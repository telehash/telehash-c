CC=gcc
CFLAGS+=-fsigned-char -DCS_1a
INCLUDE+=-Iaes -Isha1 -Isha2 -Iecc -Ihmac -Ibase64 -I../lib -I../ext -I../unix -I. -I../../js0n
LIBS+=-lrt
all:
	 $(CC) $(CFLAGS) -o original_test original_test.c aes/aes.c ecc/ecc.c hmac/hmac.c sha1/sha1.c sha2/sha256.c sha2/sha2_small_common.c platform.c $(INCLUDE) $(LIBS)
	$(CC) $(CFLAGS) -o test test.c ../../js0n/js0n.c ../../js0n/j0g.c ../lib/packet.c ../lib/util.c crypt.c crypt_1a.c base64/base64_enc.c base64/base64_dec.c aes/aes.c ecc/ecc.c hmac/hmac.c sha1/sha1.c sha2/sha256.c sha2/sha2_small_common.c platform.c $(INCLUDE) $(LIBS)
	$(CC) $(CFLAGS) -o idgen idgen.c ../../js0n/js0n.c ../../js0n/j0g.c ../lib/packet.c ../lib/util.c crypt.c crypt_1a.c base64/base64_enc.c base64/base64_dec.c aes/aes.c ecc/ecc.c hmac/hmac.c sha1/sha1.c sha2/sha256.c sha2/sha2_small_common.c platform.c $(INCLUDE) $(LIBS)
	$(CC) $(CFLAGS) -o ping ping.c ../../js0n/js0n.c ../../js0n/j0g.c ../lib/packet.c ../lib/util.c crypt.c crypt_1a.c base64/base64_enc.c base64/base64_dec.c aes/aes.c ecc/ecc.c hmac/hmac.c sha1/sha1.c sha2/sha256.c sha2/sha2_small_common.c platform.c $(INCLUDE) $(LIBS)
 
clean:
	rm -f original_test
	rm -f test
	rm -f idgen
	rm -f id.json
