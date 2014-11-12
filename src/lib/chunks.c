#include "chunks.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "platform.h"

chunks_t chunks_new(uint8_t size, uint32_t window)
{
  chunks_t chunks;
  if(!(chunks = malloc(sizeof (struct chunks_struct)))) return LOG("OOM");
  memset(chunks,0,sizeof (struct chunks_struct));

  chunks->size = size;
  chunks->window = window;
  return chunks;
}

chunks_t chunks_free(chunks_t chunks)
{
  if(!chunks) return NULL;
  if(chunks->writing) free(chunks->writing);
  if(chunks->reading) free(chunks->reading);
  free(chunks);
  return NULL;
}


