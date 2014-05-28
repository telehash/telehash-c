#ifndef packet_h
#define packet_h

#include "path.h"

// the maximum index size of how many top level elements there are in the json (each keypair is 4)
#define JSONDENSITY 64

typedef struct packet_struct
{
  unsigned char *raw;
  unsigned char *body;
  unsigned short body_len;
  unsigned char *json;
  unsigned short json_len;
  unsigned short js[JSONDENSITY];
  struct packet_struct *next, *chain;
  struct hn_struct *to, *from;
  path_t out;
  char *jsoncp; // internal editable copy of the json
} *packet_t;

// these all allocate/free memory
packet_t packet_new();
packet_t packet_copy(packet_t p);
packet_t packet_free(packet_t p); // returns NULL for convenience

// creates a new parent packet chained to the given child one, so freeing the new packet also free's it
packet_t packet_chain(packet_t child);
// manually chain together two packets
packet_t packet_link(packet_t parent, packet_t child);
// return a linked child if any
packet_t packet_linked(packet_t parent);
// returns child, unlinked
packet_t packet_unlink(packet_t parent);

// initialize json/body from raw, parses json
packet_t packet_parse(unsigned char *raw, unsigned short len);

// return raw info from stored json/body
unsigned char *packet_raw(packet_t p);
unsigned short packet_len(packet_t p);

// return current packet capacity
unsigned short packet_space(packet_t p);

// return json pointer safe to use w j0g
char *packet_j0g(packet_t p);

// set/store these in the current packet, !0 if error parsing json
int packet_json(packet_t p, unsigned char *json, unsigned short len);
unsigned char *packet_body(packet_t p, unsigned char *body, unsigned short len);
void packet_append(packet_t p, unsigned char *chunk, unsigned short len);

// convenient json setters/getters
void packet_set(packet_t p, char *key, char *val, int vlen); // raw
void packet_set_str(packet_t p, char *key, char *val); // escapes value
void packet_set_int(packet_t p, char *key, int val);
void packet_set_printf(packet_t p, char *key, const char *format, ...);

// copies keys from json into p
void packet_set_json(packet_t p, packet_t json);

// count of keys
int packet_keys(packet_t p);

// alpha sorts the json keys in the packet
void packet_sort(packet_t p);

// 0 to match, !0 if different, compares only top-level json and body
int packet_cmp(packet_t a, packet_t b);

// the return char* is invalidated with any _set* operation!
char *packet_get_str(packet_t p, char *key);
char *packet_get_istr(packet_t p, int i); // returns ["0","1","2","3"] or {"0":"1","2":"3"}

packet_t packet_get_packet(packet_t p, char *key); // creates new packet from key:object value
packet_t packet_get_packets(packet_t p, char *key); // list of packet->next from key:[object,object]

#endif