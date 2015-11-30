#ifndef e3x_h
#define e3x_h

#define E3X_VERSION_MAJOR 0
#define E3X_VERSION_MINOR 5
#define E3X_VERSION_PATCH 1
#define E3X_VERSION ((E3X_VERSION_MAJOR) * 10000 + (E3X_VERSION_MINOR) * 100 + (E3X_VERSION_PATCH))

#ifdef __cplusplus
extern "C" {
#endif

#include "lob.h" // json+binary container

// ##### e3x - end-to-end encrypted exchange #####
//
// * intended to be wrapped/embedded in other codebases
// * includes some minimal crypto, but is primarily a wrapper around other crypto libraries
// * tries to minimize the integration points to send and receive encrypted packets over any transport
// * everything contains a '3' to minimize any naming conflicts when used with other codebases
//


// top-level library functions

// process-wide boot one-time initialization, !0 is error and lob_get(options,"err");
uint8_t e3x_init(lob_t options);

// return last known error string from anywhere, intended only for debugging/logging
uint8_t *e3x_err(void);

// generate a new local identity, secrets returned in the lob json and keys in the linked lob json
lob_t e3x_generate(void);

// random bytes, from a supported cipher set
uint8_t *e3x_rand(uint8_t *bytes, size_t len);

// sha256 hashing, from one of the cipher sets
uint8_t *e3x_hash(uint8_t *in, size_t len, uint8_t *out32);


// local endpoint state management
#include "e3x_self.h"

// a single exchange (a session w/ local endpoint and remote endpoint)
#include "e3x_exchange.h"



//##############
/* binding notes

* app must have an index of the hashnames-to-exchange and tokens-to-exchange
* for each exchange, keep a list of active network transport sessions and potential transport paths
* also keep a list of active channels per exchange to track state
* one transport session may be delivering to multiple exchanges (not a 1:1 mapping)
* unreliable transport sessions must trigger a new handshake for any exchange when they need to be re-validated
* all transport sessions must signal when they are closed, which generates a new handshake to any other sessions
* always use the most recently validated-active transport session to deliver to for sending

*/

#ifdef __cplusplus
}
#endif

#endif
