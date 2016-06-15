EMCC=emcc

all: 
	cd .. && make static-cs1a
	$(EMCC) ../telehash.c -I ../include/ -s LINKABLE=1 -s EXPORT_ALL=1 -s NO_FILESYSTEM=1 -s INLINING_LIMIT=1 -s RESERVED_FUNCTION_POINTERS=1000 -O3 -o th.js

