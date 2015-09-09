#include "telehash.h"

// utilities to parse and generate minimal socket.io / engine.io packets

lob_t socketio_decode(lob_t data)
{
  // decodes one packet from the data body, adjusts body
  // sets json w/ {"etype":1,"ptype":2} and body of binary payload or linked lob for json payload
  return NULL;
}

lob_t socketio_encode(uint8_t etype, uint8_t ptype, lob_t payload)
{
  // creates lob w/ body set to encoded packet
  return NULL;
}
