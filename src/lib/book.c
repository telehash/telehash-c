#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdarg.h>

// creates new ch, optionally loads from index
chapter_t chapter_parse(const uint8_t *index)
{
  chapter_t ch;
  if(!(ch = malloc(sizeof (struct chapter_struct)))) return LOG("OOM");
  memset(ch,0,sizeof (struct chapter_struct));

  if(index) memcpy(&(ch->index),index,16);
  
  LOG_DEBUG("new book: %s",ch->index.title);
  return ch;
}

// allocs new
chapter_t chapter_new(char *title, uint16_t start, uint32_t len, uint32_t hash)
{
  if(!title || !start) return LOG_WARN("bad args");
  uint8_t tlen = strlen(title);
  if(tlen > 6) return LOG_WARN("title too long, must be <= 6: '%s'",title);
  if(len > (65535*16)) return LOG_WARN("len of %lu is too long, must be < 1048560",len);

  chapter_t ch = chapter_parse(NULL);

  memcpy(ch->index.title,title,tlen);
  ch->index.hash = hash;
  ch->index.start = start;
  ch->index.len = len;
  
  LOG_DEBUG("new book: %s",ch->index.title);
  return ch;
}

// free's local state about this chapter
chapter_t chapter_free(chapter_t ch)
{
  if(ch) free(ch);
  return LOG_DEBUG("done");
}

// accessors
uint32_t chapter_hash(chapter_t ch)
{
  if(!ch) return 0;
  return ch->index.hash;
}

uint32_t chapter_len(chapter_t ch)
{
  if(!ch) return 0;
  return ch->index.len;
}

char *chapter_title(chapter_t ch)
{
  if(!ch) return 0;
  return ch->index.title;
}

// pages
uint16_t chapter_first(chapter_t ch)
{
  if(!ch) return 0;
  return ch->index.start;
}

uint16_t chapter_last(chapter_t ch)
{
  if(!ch) return 0;
  return ch->index.start + (ch->index.len / 16);
}

// new lob describing it
lob_t chapter_json(chapter_t ch)
{
  if(!ch) return LOG_DEBUG("empty/no chapter");
  lob_t json = lob_new();
  lob_set(json,"title",ch->index.title);
  lob_set_uint(json,"start",ch->index.start);
  lob_set_uint(json,"len",ch->index.len);
  lob_set_uint(json,"hash",ch->index.hash);
  return json;
}

// accumulates checkhash and returns next page to load after given data (must be len%16==0)
uint16_t chapter_pager(chapter_t ch, uint16_t at, uint8_t *bin, uint32_t len)
{
  PMurHash32_Process(&(ch->hash), &(ch->carry), bin, len);
  uint16_t pages = (len + 16 - 1)/16;
  uint16_t max = (ch->index.len + 16 - 1)/16;
  if(at+pages >= (ch->index.start + max)) return 0;
  return at+pages;
}

// success of last full pager hashing
chapter_t chapter_verify(chapter_t ch, bool set)
{
  if(!ch) return LOG_DEBUG("no chapter");

  uint32_t hash = PMurHash32_Result(ch->hash, ch->carry, ch->index.len);
  ch->carry = ch->hash = 0;
  
  if(set)
  {
    ch->index.hash = hash;
    return ch;
  }

  return (ch->index.hash == hash)?ch:LOG_DEBUG("verify failed %u != %u",ch->index.hash,hash);
}

// given the first page, return the last page of the lob header
uint16_t chapter_head(chapter_t ch, uint8_t *first)
{
  uint16_t len = util_sys_short(*(uint16_t*)first);
  uint16_t page = (len + 16 - 1)/16;
  return ch->index.start + page;
}

// returns raw index page
uint8_t *chapter_index(chapter_t ch)
{
  return (uint8_t*)&(ch->index);
}

// set the len/hash using these contents
chapter_t chapter_set(chapter_t ch, lob_t contents)
{
  if(!ch) return LOG_DEBUG("bad args");
  if(!contents)
  {
    ch->index.len = 0;
    ch->index.hash = 0;
    return ch;
  }
  
  ch->index.len = lob_len(contents);
  ch->index.hash = murmur4(lob_raw(contents), ch->index.len);
  return ch;
}

//////// this is for making writeable books

// creates summary of chapters 
lob_t book_json(lob_t toc)
{
  lob_t json = lob_new();

  uint8_t *pages = lob_body_get(toc);
  uint32_t len = lob_body_len(toc);
  uint16_t page = 0;
  for(;(page*16) < len;page++)
  {
    chapter_t ch = chapter_parse(pages+(page*16));
    char array[32] = {0};
    snprintf(array,sizeof(array),"[%u,%u,%"PRIu32",%"PRIu32"]",chapter_first(ch),chapter_last(ch),chapter_len(ch),chapter_hash(ch));
    lob_set_raw(json, chapter_title(ch),0,array,0);
    chapter_free(ch);
  }

  return json;
}

// count of chapters
uint16_t book_chapters(lob_t toc)
{
  return lob_body_len(toc)/16;
}

// returns matching index page for given title in the toc
uint8_t *book_get(lob_t toc, char *title)
{
  if(!toc || !title) return LOG_DEBUG("bad args");
  uint8_t tlen = strlen(title);
  if(tlen > 6) return LOG_DEBUG("title too long, must be %u <= 6",tlen);

  uint8_t *pages = lob_body_get(toc);
  uint32_t len = lob_body_len(toc);
  uint16_t page = 0;
  for(;(page*16) < len;page++)
  {
    struct ndxpg_struct *index = (struct ndxpg_struct*)(pages+(page*16));
    if(memcmp(title,index->title,tlen) == 0)
    {
      return pages+(page*16);
    }
  }
  return NULL;
}

// adds chapter, fails if exists
lob_t book_add(lob_t toc, chapter_t ch)
{
  if(!toc) toc = lob_new();
  if(!ch) return toc;
  if(book_get(toc, chapter_title(ch))) return LOG_DEBUG("chapter '%s' already exists",chapter_title(ch));
  lob_append(toc, chapter_index(ch), 16);
  return toc;
}

// just updates this chapter in the toc (fails if doesn't exist)
lob_t book_mod(lob_t toc, chapter_t ch)
{
  if(!toc || !ch) return LOG_DEBUG("bad args");
  uint8_t *index = book_get(toc, chapter_title(ch));
  if(!index) return LOG_DEBUG("chapter not found: %s",chapter_title(ch));
  memcpy(index,chapter_index(ch),16);
  return toc;
}

// removes any chapter w/ this title
lob_t book_del(lob_t toc, char *title)
{
  if(!book_get(toc,title)) return LOG_DEBUG("bad args");
  uint8_t tlen = strlen(title);

  // make a temp container to rebuild body of index pages into
  lob_t tmp = lob_new();

  uint8_t *pages = lob_body_get(toc);
  uint32_t len = lob_body_len(toc);
  uint16_t page = 0;
  for(;(page*16) < len;page++)
  {
    struct ndxpg_struct *index = (struct ndxpg_struct*)(pages+(page*16));
    if(memcmp(title,index->title,tlen) != 0)
    {
      lob_append(tmp,pages+(page*16),16);
    }
  }
  
  lob_body(toc,lob_body_get(tmp),lob_body_len(tmp));
  return toc;
}

// provide current chapter hash of the toc
uint32_t book_hash(lob_t toc)
{
  return murmur4(lob_raw(toc),lob_len(toc));
}
