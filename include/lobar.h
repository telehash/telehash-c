#ifndef lobar_h
#define lobar_h

#include <stdint.h>
#include <stdlib.h>

/****** Length Object Binary ARchive ******

* mental model is of books with pages organized into chapters and a toc
* page size is fixed (16b) with max pages being 2^16 (1MB)
* toc and chapters are always a raw LOB
* toc is always the first pages in a book
* app uses chapter titles to organize (chapters and appendices)
* index page 16 byte format: [mmmm][llll][ss][tttttt]
  * mmmm - murmurhash32
  * llll - byte length of chapter
  * ss - start page
  * tttttt - max 6 char chapter title
* any chapter raw data pages may be addressed w/ 2 bytes for the page id
*******************************************/

// raw index format
struct index_struct
{
  uint32_t hash;
  uint32_t len;
  uint16_t start;
  uint8_t title[6];
};

// chapter meta tracker
typedef struct lobar_struct
{
  uint32_t pager; // cumulative hash during paging
  struct index_struct index;
  uint8_t null; // makes sure index.title is null terminated
} *lobar_t;


// creates new chapter from source lob w/ given title (optional overriding total length)
lobar_t lobar_create(lob_t source, char *title, uint16_t start, uint32_t len);

// loads chapter from a raw index page
lobar_t lobar_parse(const uint8_t *index);

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
uint16_t lobar_pager(lobar_t chapter, uint16_t at, uint8_t *bin, uint32_t len);

// success of last full pager hashing
lobar_t lobar_verify(lobar_t chapter);

// given the first page, return the last page of the lob header
uint16_t lobar_head(lobar_t chapter, uint8_t *first);

// returns raw index page
uint8_t *lobar_index(lobar_t chapter);

//////// this is for making writeable books

// creates summary of chapters 
lob_t lobar_toc_json(lob_t toc);

// finds a block of pages w/ this much space, returns first block
uint16_t lobar_toc_space(lob_t toc, uint32_t len);

// loads chapter from a toc lob by matching name
lobar_t lobar_toc_get(lob_t toc, char *title);

// adds chapter, fails if exists
lob_t lobar_toc_add(lob_t toc, lobar_t chapter);

// just updates this chapter in the toc (fails if doesn't exist)
lob_t lobar_toc_mod(lob_t toc, lobar_t chapter);

// removes any chapter w/ this title
lob_t lobar_toc_del(lob_t toc, char *title);


#endif
