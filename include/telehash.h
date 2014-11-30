#ifndef telehash_h
#define telehash_h

#define TELEHASH_VERSION_MAJOR 3
#define TELEHASH_VERSION_MINOR 0
#define TELEHASH_VERSION_PATCH 10
#define TELEHASH_VERSION ((TELEHASH_VERSION_MAJOR) * 10000 + (TELEHASH_VERSION_MINOR) * 100 + (TELEHASH_VERSION_PATCH))

#ifdef __cplusplus
extern "C" {
#endif

#include "mesh.h"
#include "link.h"
#include "links.h"
#include "platform.h"
#include "pipe.h"
#include "e3x.h"
#include "lib.h"

#ifdef __cplusplus
}
#endif

#endif