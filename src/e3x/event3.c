#include <string.h>
#include "event3.h"
#include "xht.h"
#include "platform.h"

struct event3_struct
{
  lob_t events;
  xht_t ids;
};

// track a list of of future-scheduled events
event3_t event3_new(uint32_t prime)
{
  event3_t ev;

  if(!(ev = malloc(sizeof (struct event3_struct)))) return NULL;
  memset(ev,0,sizeof (struct event3_struct));
  
  if(prime) ev->ids = xht_new(prime);

  return ev;
}

void event3_free(event3_t ev)
{
  lob_t event, old;
  if(!ev) return;
  // free all events in the list
  event = ev->events;
  while(event)
  {
    old = event;
    event = event->next;
    LOG("unused event %s",lob_get(old,"id"));
    lob_free(old);
  }
  if(ev->ids) xht_free(ev->ids);
  free(ev);
}

// the next lowest at value, or 0 if none
uint32_t event3_at(event3_t ev)
{
  if(!ev || !ev->events) return 0;
  return ev->events->quota;
}

// remove and return the lowest lob packet below the given at
lob_t event3_get(event3_t ev, uint32_t at)
{
  lob_t next;
  if(!ev || !ev->events || !at) return NULL;
  if(ev->events->quota > at) return NULL;
  next = ev->events;
  ev->events = next->next;
  next->next = next->chain = NULL;
  if(ev->events) ev->events->chain = NULL;
  return next;
}

// 0 is delete, event is unique per id
void event3_set(event3_t ev, lob_t event, char *id, uint32_t at)
{
  // save at to event->quota
  // use event->chain as previous for linked list
  // if no ev->ids ignore id
}

