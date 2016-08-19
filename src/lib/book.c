#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
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
  return ch->index.hash + (ch->index.len / 16);
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
  return LOG_INFO("TODO");
}

// finds a block of pages w/ this much space, returns first block
uint16_t book_space(lob_t toc, uint32_t len)
{
  LOG_INFO("TODO");
  return 0;
}

// loads chapter from a toc lob by matching name
chapter_t book_get(lob_t toc, char *title)
{
  return LOG_INFO("TODO");
}

// adds chapter, fails if exists
lob_t book_add(lob_t toc, chapter_t chapter)
{
  return LOG_INFO("TODO");
}

// just updates this chapter in the toc (fails if doesn't exist)
lob_t book_mod(lob_t toc, chapter_t chapter)
{
  return LOG_INFO("TODO");
}

// removes any chapter w/ this title
lob_t book_del(lob_t toc, char *title)
{
  return LOG_INFO("TODO");
}

// provide current chapter hash of the toc
uint32_t book_hash(lob_t toc)
{
  return 0;
}
