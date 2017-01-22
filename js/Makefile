EMCC=emcc

all: 
	cd .. && make static-cs1c
	$(EMCC) ../telehash.c -I ../include/ --pre-js module.js --memory-init-file 0 -s ALLOW_MEMORY_GROWTH=1 -s LINKABLE=1 -s EXPORT_ALL=1 -s NO_FILESYSTEM=1 -s NO_DYNAMIC_EXECUTION=1 -s INLINING_LIMIT=1 -s RESERVED_FUNCTION_POINTERS=1000 -O3 -o include/thc.js

test: all
	node samples/test.js