all:
	gcc -Wall -o test test.c -ltomcrypt -ltommath -DLTM_DESC
