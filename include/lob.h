#ifndef lob_h
#define lob_h

#include <stdint.h>
#include <stdlib.h>

typedef struct lob_struct
{
  // these are public but managed by accessors
  uint8_t *raw;
  uint8_t *body;
  size_t body_len;
  uint8_t *head;
  size_t head_len;
  
  // these are all for external use only
  uint32_t id;
  void *arg;

  // these are internal/private
  struct lob_struct *chain;
  char *cache; // edited copy of the json head

  // used only by the list utils
  struct lob_struct *next, *prev;

} *lob_t;

// these all allocate/free memory
lob_t lob_new();
lob_t lob_copy(lob_t p);
lob_t lob_free(lob_t p); // returns NULL for convenience

// creates a new parent packet chained to the given child one, so freeing the new packet also free's it
lob_t lob_chain(lob_t child);
// manually chain together two packets, returns parent, frees any existing child, creates parent if none
lob_t lob_link(lob_t parent, lob_t child);
// return a linked child if any
lob_t lob_linked(lob_t parent);
// returns child, unlinked
lob_t lob_unlink(lob_t parent);

// initialize head/body from raw, parses json
lob_t lob_parse(const uint8_t *raw, size_t len);

// return full encoded packet
uint8_t *lob_raw(lob_t p);
size_t lob_len(lob_t p);

// return null-terminated json header only
char *lob_json(lob_t p);

// set/store these in the current packet
uint8_t *lob_head(lob_t p, uint8_t *head, size_t len);
uint8_t *lob_body(lob_t p, uint8_t *body, size_t len);
lob_t lob_append(lob_t p, uint8_t *chunk, size_t len);
lob_t lob_append_str(lob_t p, char *chunk);

// convenient json setters/getters, always return given lob so they're chainable
lob_t lob_set_raw(lob_t p, char *key, size_t klen, char *val, size_t vlen); // raw
lob_t lob_set(lob_t p, char *key, char *val); // escapes value
lob_t lob_set_len(lob_t p, char *key, size_t klen, char *val, size_t vlen); // same as lob_set
lob_t lob_set_int(lob_t p, char *key, int val);
lob_t lob_set_uint(lob_t p, char *key, unsigned int val);
lob_t lob_set_float(lob_t p, char *key, float val, uint8_t places);
lob_t lob_set_printf(lob_t p, char *key, const char *format, ...);
lob_t lob_set_base32(lob_t p, char *key, uint8_t *val, size_t vlen);

// copies keys from json into p
lob_t lob_set_json(lob_t p, lob_t json);

// count of keys
unsigned int lob_keys(lob_t p);

// alpha-sorts the json keys
lob_t lob_sort(lob_t p);

// 0 to match, !0 if different, compares only top-level json and body
int lob_cmp(lob_t a, lob_t b);

// the return uint8_t* is invalidated with any _set* operation!
char *lob_get(lob_t p, char *key);
int lob_get_int(lob_t p, char *key);
unsigned int lob_get_uint(lob_t p, char *key);
float lob_get_float(lob_t p, char *key);

char *lob_get_index(lob_t p, uint32_t i); // returns ["0","1","2","3"] or {"0":"1","2":"3"}

// just shorthand for util_cmp to match a key/value
int lob_get_cmp(lob_t p, char *key, char *val);

// get the raw value, must use get_len
char *lob_get_raw(lob_t p, char *key);
size_t lob_get_len(lob_t p, char *key);

// returns new packets based on values
lob_t lob_get_json(lob_t p, char *key); // creates new packet from key:object value
lob_t lob_get_array(lob_t p, char *key); // list of packet->next from key:[object,object]
lob_t lob_get_base32(lob_t p, char *key); // decoded binary is the return body

// handles cloaking conveniently, len is lob_len()+(8*rounds)
uint8_t *lob_cloak(lob_t p, uint8_t rounds);

// decloaks and parses
lob_t lob_decloak(uint8_t *cloaked, size_t len);

// TODO, this would be handy, js syntax to get a json value
// char *lob_eval(lob_t p, "foo.bar[0]['zzz']");

// manage a basic double-linked list of packets using ->next and ->prev
lob_t lob_pop(lob_t list); // returns last item, item->next is the new list
lob_t lob_push(lob_t list, lob_t append); // appends new item, returns new list
lob_t lob_shift(lob_t list); // returns first item, item->next is the new list
lob_t lob_unshift(lob_t list, lob_t prepend); // adds item, returns new list
lob_t lob_splice(lob_t list, lob_t extract); // removes item from list, returns new list
lob_t lob_insert(lob_t list, lob_t after, lob_t p); // inserts item in list after other item, returns new list
lob_t lob_freeall(lob_t list); // frees all
lob_t lob_match(lob_t list, char *key, char *value); // find the first packet in the list w/ the matching key/value
lob_t lob_next(lob_t list);
lob_t lob_array(lob_t list); // return json array of the list

#endif
