all:
	gcc -Wall -o test test.c -ltomcrypt -ltommath -DLTM_DESC
	gcc -Wall -o idgen idgen.c -ltomcrypt -ltommath -DLTM_DESC
