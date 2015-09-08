#ifndef socketio_h
#define socketio_h

// utilities to parse and generate minimal socket.io / engine.io packets

#include <stdint.h>
#include "lob.h"

// https://github.com/Automattic/engine.io-protocol
#define SOCKETIO_ETYPE_OPEN 0
#define SOCKETIO_ETYPE_CLOSE 1
#define SOCKETIO_ETYPE_PING 2
#define SOCKETIO_ETYPE_PONG 3
#define SOCKETIO_ETYPE_MESSAGE 4
#define SOCKETIO_ETYPE_UPGRADE 5
#define SOCKETIO_ETYPE_NOOP 6

// https://github.com/Automattic/socket.io-protocol
#define SOCKETIO_PTYPE_CONNECT 0
#define SOCKETIO_PTYPE_DISCONNECT 1
#define SOCKETIO_PTYPE_EVENT 2
#define SOCKETIO_PTYPE_ACK 3
#define SOCKETIO_PTYPE_ERROR 4
#define SOCKETIO_PTYPE_BINARY_EVENT 5
#define SOCKETIO_PTYPE_BINARY_ACK 6


lob_t socketio_decode(lob_t data);

lob_t socketio_encode(uint8_t etype, uint8_t ptype, lob_t payload);

#endif
