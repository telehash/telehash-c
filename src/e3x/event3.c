#include <string.h>
#include "e3x.h"
#include "xht.h"
#include "platform.h"

struct event3_struct
{
  lob_t events;
  xht_t ids;
  uint64_t now;
};

// track a list of of future-scheduled events
event3_t event3_new(uint32_t prime, uint64_t now)
{
  event3_t ev;

  if(!(ev = malloc(sizeof (struct event3_struct)))) return NULL;
  memset(ev,0,sizeof (struct event3_struct));
  
  ev->now = now;
  if(prime) ev->ids = xht_new(prime);
  
  return ev;
}

void event3_free(event3_t ev)
{
  if(!ev) return;
  // free all events in the list
  if(ev->events) LOG("freeing unused events");
  lob_freeall(ev->events);
  if(ev->ids) xht_free(ev->ids);
  free(ev);
}

// the next lowest at value, or 0 if none
uint32_t event3_at(event3_t ev, uint64_t now)
{
  if(!ev || !ev->events) return UINT32_MAX;
  // TODO rebase!
  ev->now = now;
  return ev->events->id;
}

// remove and return the lowest lob packet below the given at
lob_t event3_get(event3_t ev)
{
  lob_t ret;
  // no events waiting or none due yet
  if(!ev || !(ret = lob_shift(ev->events))) return NULL;
  ev->events = ret->next;
  return ret;
}

// 0 is delete, event is unique per id
void event3_set(event3_t ev, lob_t event, char *id, uint32_t at)
{
  lob_t existing;
  if(!ev) return;

  // if given an id, and there's a different existing lob, remove it
  if(id)
  {
    existing = xht_get(ev->ids,id);
    if(existing && existing != event)
    {
      LOG("replacing existing event timer %s",id);
      ev->events = lob_splice(ev->events, existing);
      lob_free(existing);
    }
  }
  
  // make sure it's not already linked somewhere
  ev->events = lob_splice(ev->events, event);

  // re-link if requested
  if(event && at)
  {
    event->id = at;

    // save id ref if requested
    if(id) xht_set(ev->ids,id,(void*)event);
    
    // is it the only one?
    if(!ev->events)
    {
      ev->events = event;
      return;
    }

    // is it the first one?
    if(at < ev->events->id)
    {
      ev->events = lob_unshift(ev->events, event);
      return;
    }

    // find where it will go in the list
    existing = ev->events;
    while(existing->next && existing->next->id < at) existing = existing->next;

    // inserts it after an existing one
    ev->events = lob_insert(ev->events, existing, event);
  }
  
}

