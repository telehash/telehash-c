all:
	gcc -o test test.c -ltomcrypt -ltommath -DLTM_DESC
	gcc -Wall -o idgen idgen.c -ltomcrypt -ltommath -DLTM_DESC
	gcc -Wall -o openize openize.c ../js0n/js0n.c ../js0n/j0g.c -ltomcrypt -ltommath -DLTM_DESC -I../js0n
