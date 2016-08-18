#ifndef book_h
#define book_h

#include <stdint.h>
#include <stdbool.h>
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
* 0 is invalid page id, it is always the book toc index page

typical flow, old is previous book, new is updated one:
- new emits updated toc hash
- old emits current toc index page
- new performs diff of toc chapter, sends old the diffset
- old copies current toc chapter and applies updated diffset
- old validates updated index page match before continuing
- old compares all chapter indexes, emits current and updated chapter index pages to new
- new performs diff of chapter, sends old the diffset
- ... copy, apply, validate, repeat
*******************************************/

// raw index page format (16 bytes)
struct index_struct
{
  uint32_t hash;
  uint32_t len;
  uint16_t start;
  char title[6];
};

// chapter meta tracker
typedef struct chapter_struct
{
  uint32_t pager; // cumulative hash during paging
  struct index_struct index;
  uint8_t null; // makes sure index.title is null terminated
} *chapter_t;

// allocates new chapter tracker w/ given values
chapter_t chapter_new(char *title, uint16_t start, uint32_t len, uint32_t hash);

// loads chapter from a raw index page
chapter_t chapter_parse(const uint8_t *index);

// free's local state about this chapter
chapter_t chapter_free(chapter_t ch);

// accessors
uint32_t chapter_hash(chapter_t ch);
uint32_t chapter_len(chapter_t ch);
char *chapter_title(chapter_t ch);

// pages
uint16_t chapter_first(chapter_t ch);
uint16_t chapter_last(chapter_t ch);

// new lob describing it
lob_t chapter_json(chapter_t ch);

// accumulates checkhash and returns next page to load after given data (must be len%16==0)
uint16_t chapter_pager(chapter_t ch, uint16_t at, uint8_t *bin, uint32_t len);

// success of last full pager hashing, optional flag to update w/ new hash
chapter_t chapter_verify(chapter_t ch, bool update);

// given the first page, return the last page of the lob header
uint16_t chapter_head(chapter_t ch, uint8_t *first);

// returns raw index page
uint8_t *chapter_index(chapter_t ch);

// set the len/hash using these contents
chapter_t chapter_set(chapter_t ch, lob_t contents);

//////// this is for making writeable books

// creates summary of chapters 
lob_t book_json(lob_t toc);

// finds a block of pages w/ this much space, returns first block
uint16_t book_space(lob_t toc, uint32_t len);

// loads chapter from a toc lob by matching name
chapter_t book_get(lob_t toc, char *title);

// adds chapter, fails if exists
lob_t book_add(lob_t toc, chapter_t chap);

// just updates this chapter in the toc (fails if doesn't exist)
lob_t book_mod(lob_t toc, chapter_t chap);

// removes any chapter w/ this title
lob_t book_del(lob_t toc, char *title);


#endif
