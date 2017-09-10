add_definitions(
    -D_GNU_SOURCE
)

set(LIB_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/lib
)

include_directories(
    ${LIB_INCLUDES}
)


set(LIB_SOURCES
  src/lib/lob.c
  src/lib/hashname.c
  src/lib/xht.c
  src/lib/js0n.c
  src/lib/base32.c
  src/lib/chacha.c
  src/lib/murmur.c
  src/lib/jwt.c
  src/lib/base64.c
  src/lib/aes128.c
  src/lib/sha256.c
  src/lib/uECC.c)
set(E3X_SOURCES src/e3x/e3x.c src/e3x/self.c src/e3x/exchange.c src/e3x/cipher.c)
set(CIPHER_SOURCES src/e3x/cs1c/cs1c.c src/e3x/cs3a_disabled.c)
set(MESH_SOURCES src/mesh.c src/link.c src/chan.c)
set(UTIL_SOURCES src/util/util.c src/util/chunks.c src/util/frames.c src/unix/util.c src/unix/util_sys.c)

add_library(telehash ${LIB_SOURCES} ${E3X_SOURCES} ${MESH_SOURCES} ${UTIL_SOURCES} ${CIPHER_SOURCES})
add_library(telehash_bl ${LIB_SOURCES} ${E3X_SOURCES} ${MESH_SOURCES} ${UTIL_SOURCES} ${CIPHER_SOURCES})
target_compile_definitions(telehash_bl PUBLIC NOLOG=true)