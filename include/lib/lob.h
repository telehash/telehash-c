#ifndef lob_h
#define lob_h

#include <stdint.h>
#include <stdlib.h>

typedef struct lob_struct
{
  // these are public but managed by accessors
  uint8_t *raw;
  uint8_t *body;
  uint32_t body_len;
  uint8_t *head;
  uint16_t head_len;
  
  // these are all for external use only
  struct lob_struct *next, *prev;
  uint32_t id;
  void *arg;

  // these are internal/private
  struct lob_struct *chain;
  char *cache; // edited copy of the json head

} *lob_t;

// these all allocate/free memory
lob_t lob_new();
lob_t lob_copy(lob_t p);
lob_t lob_free(lob_t p); // returns NULL for convenience

// creates a new parent packet chained to the given child one, so freeing the new packet also free's it
lob_t lob_chain(lob_t child);
// manually chain together two packets
lob_t lob_link(lob_t parent, lob_t child);
// return a linked child if any
lob_t lob_linked(lob_t parent);
// returns child, unlinked
lob_t lob_unlink(lob_t parent);

// initialize head/body from raw, parses json
lob_t lob_parse(uint8_t *raw, uint32_t len);

// return full encoded packet
uint8_t *lob_raw(lob_t p);
uint32_t lob_len(lob_t p);

// return null-terminated json header only
char *lob_json(lob_t p);

// set/store these in the current packet
uint8_t *lob_head(lob_t p, uint8_t *head, uint16_t len);
uint8_t *lob_body(lob_t p, uint8_t *body, uint32_t len);
lob_t lob_append(lob_t p, uint8_t *chunk, uint32_t len);

// convenient json setters/getters, always return given lob so they're chainable
lob_t lob_set_raw(lob_t p, char *key, uint16_t klen, char *val, uint16_t vlen); // raw
lob_t lob_set(lob_t p, char *key, char *val); // escapes value
lob_t lob_set_int(lob_t p, char *key, int val);
lob_t lob_set_printf(lob_t p, char *key, const char *format, ...);
lob_t lob_set_base32(lob_t p, char *key, uint8_t *val, uint16_t vlen);

// copies keys from json into p
lob_t lob_set_json(lob_t p, lob_t json);

// count of keys
int lob_keys(lob_t p);

// alpha-sorts the json keys
lob_t lob_sort(lob_t p);

// 0 to match, !0 if different, compares only top-level json and body
int lob_cmp(lob_t a, lob_t b);

// the return uint8_t* is invalidated with any _set* operation!
char *lob_get(lob_t p, char *key);
int lob_get_int(lob_t p, char *key);
char *lob_get_index(lob_t p, uint32_t i); // returns ["0","1","2","3"] or {"0":"1","2":"3"}

// just shorthand for util_cmp to match a key/value
int lob_get_cmp(lob_t p, char *key, char *val);

// get the raw value, must use get_len
char *lob_get_raw(lob_t p, char *key);
uint32_t lob_get_len(lob_t p, char *key);

// returns new packets based on values
lob_t lob_get_json(lob_t p, char *key); // creates new packet from key:object value
lob_t lob_get_array(lob_t p, char *key); // list of packet->next from key:[object,object]
lob_t lob_get_base32(lob_t p, char *key); // decoded binary is the return body

// handles cloaking conveniently, len is lob_len()+(8*rounds)
uint8_t *lob_cloak(lob_t p, uint8_t rounds);

// decloaks and parses
lob_t lob_decloak(uint8_t *cloaked, uint32_t len);

// TODO, this would be handy, js syntax to get a json value
// char *lob_eval(lob_t p, "foo.bar[0]['zzz']");

#endif
