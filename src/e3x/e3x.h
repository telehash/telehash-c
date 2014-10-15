#ifndef e3x_h
#define e3x_h

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
uint8_t *e3x_rand(uint8_t *bytes, uint32_t len);

// sha256 hashing, from one of the cipher sets
uint8_t *e3x_hash(uint8_t *in, uint32_t len, uint8_t *out32);


// local endpoint state management
#include "self3.h"

// a single exchange (a session w/ local endpoint and remote endpoint)
#include "exchange3.h"

// standalone timer event utility for channels
#include "event3.h"

// standalone channel packet buffer/ordering utility
#include "channel3.h"



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

#endif