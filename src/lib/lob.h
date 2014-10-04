#ifndef lob_h
#define lob_h

#include <stdint.h>
#include <stdlib.h>

typedef struct lob_struct
{
  uint8_t *raw;
  uint8_t *body;
  uint32_t body_len;
  uint8_t *head;
  uint16_t head_len;
  struct lob_struct *next, *chain;
  uint16_t *index; // js0n parse output
  uint8_t *json; // internal editable copy of the json
  uint16_t quota; // defaults to 1440
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

// return current packet capacity based on quota
uint16_t lob_space(lob_t p);

// return json pointer safe to use w j0g
char *lob_j0g(lob_t p);

// set/store these in the current packet, !0 if error parsing json
int lob_json(lob_t p, uint8_t *json, uint16_t len);
uint8_t *lob_body(lob_t p, uint8_t *body, uint16_t len);
void lob_append(lob_t p, uint8_t *chunk, uint16_t len);

// convenient json setters/getters
void lob_set_raw(lob_t p, char *key, uint8_t val, uint16_t vlen); // raw
void lob_set(lob_t p, char *key, uint8_t *val); // escapes value
void lob_set_int(lob_t p, char *key, int val);
void lob_set_printf(lob_t p, char *key, const char *format, ...);
uint8_t lob_set_base32(lob_t p, char *key, uint8_t val, uint16_t vlen);

// copies keys from json into p
void lob_set_json(lob_t p, lob_t json);

// count of keys
int lob_keys(lob_t p);

// alpha sorts the json keys in the packet
void lob_sort(lob_t p);

// 0 to match, !0 if different, compares only top-level json and body
int lob_cmp(lob_t a, lob_t b);

// the return uint8_t* is invalidated with any _set* operation!
uint8_t *lob_get(lob_t p, char *key);
uint8_t *lob_get_istr(lob_t p, uint8_t i); // returns ["0","1","2","3"] or {"0":"1","2":"3"}

// returns new packets based on values
lob_t lob_get_packet(lob_t p, char *key); // creates new packet from key:object value
lob_t lob_get_packets(lob_t p, char *key); // list of packet->next from key:[object,object]
lob_t lob_get_base32(lob_t p, char *key); // decoded binary is the return body

#endif
