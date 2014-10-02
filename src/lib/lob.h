#ifndef lob_h
#define lob_h

// the maximum index size of how many top level elements there are in the json (each keypair is 4)
#define JSONDENSITY 64

typedef struct lob_struct
{
  unsigned char *raw;
  unsigned char *body;
  unsigned short body_len;
  unsigned char *json;
  unsigned short json_len;
  unsigned short js[JSONDENSITY];
  struct lob_struct *next, *chain;
  char *jsoncp; // internal editable copy of the json
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

// initialize json/body from raw, parses json
lob_t lob_parse(unsigned char *raw, unsigned short len);

// return raw info from stored json/body
unsigned char *lob_raw(lob_t p);
unsigned short lob_len(lob_t p);

// return current packet capacity
unsigned short lob_space(lob_t p);

// return json pointer safe to use w j0g
char *lob_j0g(lob_t p);

// set/store these in the current packet, !0 if error parsing json
int lob_json(lob_t p, unsigned char *json, unsigned short len);
unsigned char *lob_body(lob_t p, unsigned char *body, unsigned short len);
void lob_append(lob_t p, unsigned char *chunk, unsigned short len);

// convenient json setters/getters
void lob_set(lob_t p, char *key, char *val, int vlen); // raw
void lob_set_str(lob_t p, char *key, char *val); // escapes value
void lob_set_int(lob_t p, char *key, int val);
void lob_set_printf(lob_t p, char *key, const char *format, ...);

// copies keys from json into p
void lob_set_json(lob_t p, lob_t json);

// count of keys
int lob_keys(lob_t p);

// alpha sorts the json keys in the packet
void lob_sort(lob_t p);

// 0 to match, !0 if different, compares only top-level json and body
int lob_cmp(lob_t a, lob_t b);

// the return char* is invalidated with any _set* operation!
char *lob_get_str(lob_t p, char *key);
char *lob_get_istr(lob_t p, int i); // returns ["0","1","2","3"] or {"0":"1","2":"3"}

lob_t lob_get_packet(lob_t p, char *key); // creates new packet from key:object value
lob_t lob_get_packets(lob_t p, char *key); // list of packet->next from key:[object,object]

#endif