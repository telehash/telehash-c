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
uint32_t event3_at(event3_t ev, uint64_t now)
{
  if(!ev || !ev->events) return UINT32_MAX;
  // TODO rebase!
  ev->now = now;
  return ev->events->id;
}

// unlink in place consistently
lob_t _unlink(event3_t ev, lob_t existing)
{
  if(!existing) return existing;
  if(existing->next) existing->next->prev = existing->prev;
  if(existing->prev) existing->prev->next = existing->next;
  if(ev->events == existing) ev->events = existing->next;
  existing->next = existing->prev = NULL;
  return existing;
}

// remove and return the lowest lob packet below the given at
lob_t event3_get(event3_t ev)
{
  // no events waiting or none due yet
  if(!ev || !ev->events || ev->events->id) return NULL;
  return _unlink(ev, ev->events);
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
      _unlink(ev, existing); // unlink in place
      lob_free(existing);
    }
  }
  
  // make sure it's not already linked somewhere
  _unlink(ev, event);

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
      event->next = ev->events;
      ev->events->prev = event;
      ev->events = event;
      return;
    }

    // find where it will go in the list
    existing = ev->events;
    while(existing->next && existing->next->id < at) existing = existing->next;
    
    // hit the end of the list
    if(!existing->next)
    {
      existing->next = event;
      event->prev = existing;
      return;
    }
    
    // insert here, we know we're in the list somewhere safely from above checks
    event->next = existing->next;
    existing->next->prev = event;
    event->prev = existing;
    existing->next = event;
  }
  
}

