#ifndef lobar_h
#define lobar_h

#include <stdint.h>
#include <stdlib.h>

// raw index format
struct index_struct
{
  uint32_t hash;
  uint32_t len;
  uint16_t start;
  uint8_t title[6];
};

// index format
typedef struct lobar_struct
{
  lob_t toc; // only used when writing
  uint32_t pager; // cumulative hash during paging
  struct index_struct index;
  uint8_t null; // makes sure index.title is null terminated
} *lobar_t;


// loads chapter from a raw index page
lobar_t lobar_chapter(const uint8_t *bin);

// loads chapter from a toc lobar by matching name
lobar_t lobar_get(lobar_t toc, char *title);

// free's local state about this chapter
lobar_t lobar_free(lobar_t chapter);

// accessors
uint32_t lobar_hash(lobar_t chapter);
uint32_t lobar_len(lobar_t chapter);
char *lobar_title(lobar_t chapter);

// pages
uint16_t lobar_first(lobar_t chapter);
uint16_t lobar_last(lobar_t chapter);

// new lob describing it
lob_t lobar_json(lobar_t chapter);

// accumulates checkhash and returns next page to load after given data (must be len%16==0)
uint16_t lobar_pager(lobar_t chapter, uint16_t page, uint8_t *bin, uint32_t len);

// success of last full pager hashing
lobar_t lobar_verify(lobar_t chapter);

// given the first page, return the last page of the lob header
uint16_t lobar_head(lobar_t chapter, uint8_t *bin);

// returns raw index page
uint8_t *lobar_index(lobar_t chapter);

//////// this is for making writeable books

lobar_t lobar_update(lobar_t chapter, uint32_t hash, uint32_t len); // changes these values in this chapter

// toc editing
lobar_t lobar_toc(lob_t book, char *title); // creates new toc from book json
lob_t lobar_toc_json(lobar_t toc); // creates summary of chapters 
uint16_t lobar_toc_space(lobar_t toc, uint32_t len); // finds a block w/ this much space
lobar_t lobar_toc_add(lobar_t toc, uint16_t page, char *title); // returns new chapter lobar
lobar_t lobar_toc_del(lobar_t toc, char *title); // removes any chapter w/ this title
lobar_t lobar_toc_mod(lobar_t toc, lobar_t chapter); // modifies existing chapter

#endif
