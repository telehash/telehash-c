#include <string.h>
#include "event3.h"
#include "../lib/xht.h"
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
  
  // leave a blank at the start
  ev->events = lob_new();
  ev->events->id = 0;

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
  if(!ev || !ev->events->next) return 0;
  return ev->events->next->id;
}

// remove and return the lowest lob packet below the given at
lob_t event3_get(event3_t ev, uint32_t at)
{
  lob_t ret;
  if(!ev || !ev->events->next || !at) return NULL;
  ret = ev->events->next;
  
  // if it's not up yet
  if(ret->id > at) return NULL;
  
  // repair the links
  ev->events->next = ret->next;
  if(ret->next) ret->next->prev = ev->events;
  
  // isolate the one we're returning
  ret->next = ret->prev = NULL;
  return ret;
}

// 0 is delete, event is unique per id
void event3_set(event3_t ev, lob_t event, char *id, uint32_t at)
{
  lob_t existing;
  if(!ev) return;
  if(event) event->id = at;

  // if given an id, and there's a different existing lob, free it
  if(id)
  {
    existing = xht_get(ev->ids,id);
    if(existing && existing != event)
    {
      // unlink in place
      if(existing->next) existing->next->prev = existing->prev;
      existing->prev->next = existing->next;
      lob_free(existing);
    }
  }

  // save new one if requested
  if(event && at)
  {
    // save id ref if requested
    if(id) xht_set(ev->ids,id,(void*)event);
    
    // place in sorted linked list
    existing = ev->events;
    while(existing->next && existing->next->id < at) existing = existing->next;
    event->next = existing->next;
    event->next->prev = event;
    existing->next = event;
    event->prev = existing;
  }
  
}

