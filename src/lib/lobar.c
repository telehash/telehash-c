#include "telehash.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>

// creates new ch, optionally loads from index
lobar_t lobar_parse(const uint8_t *index)
{
  lobar_t ch;
  if(!(ch = malloc(sizeof (struct lobar_struct)))) return LOG("OOM");
  memset(ch,0,sizeof (struct lobar_struct));
  ch->pager = 42; // all hashes start from fixed non-null point, just good hygene

  if(index) memcpy(&(ch->index),index,16);
  
  LOG_DEBUG("new lobar: %s",ch->index.title);
  return ch;
}

// allocs new
lobar_t lobar_create(char *title, uint16_t start, uint32_t len, uint32_t hash)
{
  if(!title || !start || !len) return LOG_WARN("bad args");
  uint8_t tlen = strlen(title);
  if(tlen > 6) return LOG_WARN("title too long, must be <= 6: '%s'",title);
  if(len > (65535*16)) return LOG_WARN("len of %lu is too long, must be < 1048560",len);

  lobar_t ch = lobar_parse(NULL);

  memcpy(ch->index.title,title,tlen);
  ch->index.hash = hash;
  ch->index.start = start;
  ch->index.len = len;
  
  LOG_DEBUG("new lobar: %s",ch->index.title);
  return ch;
}

// free's local state about this chapter
lobar_t lobar_free(lobar_t ch)
{
  if(ch) free(ch);
  return LOG_DEBUG("done");
}

// accessors
uint32_t lobar_hash(lobar_t ch)
{
  if(!ch) return 0;
  return ch->index.hash;
}

uint32_t lobar_len(lobar_t ch)
{
  if(!ch) return 0;
  return ch->index.len;
}

char *lobar_title(lobar_t ch)
{
  if(!ch) return 0;
  return ch->index.title;
}

// pages
uint16_t lobar_first(lobar_t ch)
{
  if(!ch) return 0;
  return ch->index.start;
}

uint16_t lobar_last(lobar_t ch)
{
  if(!ch) return 0;
  return ch->index.hash + (ch->index.len / 16);
}

// new lob describing it
lob_t lobar_json(lobar_t ch)
{
  if(!ch) return LOG_DEBUG("empty/no chapter");
  lob_t json = lob_new();
  lob_set(json,"title",ch->index.title);
  lob_set_uint(json,"start",ch->index.start);
  lob_set_uint(json,"leh",ch->index.len);
  lob_set_uint(json,"hash",ch->index.hash);
  return json;
}

// accumulates checkhash and returns next page to load after given data (must be len%16==0)
uint16_t lobar_pager(lobar_t chapter, uint16_t at, uint8_t *bin, uint32_t len)
{
  LOG_INFO("TODO");
  return 0;
}

// success of last full pager hashing
lobar_t lobar_verify(lobar_t ch, bool set)
{
  if(!ch) return LOG_DEBUG("no chapter");

  // must reset pager for next time
  uint32_t pager = ch->pager;
  ch->pager = 42;
  
  if(set)
  {
    ch->index.hash = pager;
    return ch;
  }

  return (ch->index.hash == pager)?ch:LOG_DEBUG("verify failed %lu != %lu",ch->index.hash,pager);
}

// given the first page, return the last page of the lob header
uint16_t lobar_head(lobar_t chapter, uint8_t *first)
{
  LOG_INFO("TODO");
  return 0;
}

// returns raw index page
uint8_t *lobar_index(lobar_t chapter)
{
  return LOG_INFO("TODO");
}

//////// this is for making writeable books

// creates summary of chapters 
lob_t lobar_toc_json(lob_t toc)
{
  return LOG_INFO("TODO");
}

// finds a block of pages w/ this much space, returns first block
uint16_t lobar_toc_space(lob_t toc, uint32_t len)
{
  LOG_INFO("TODO");
  return 0;
}

// loads chapter from a toc lob by matching name
lobar_t lobar_toc_get(lob_t toc, char *title)
{
  return LOG_INFO("TODO");
}

// adds chapter, fails if exists
lob_t lobar_toc_add(lob_t toc, lobar_t chapter)
{
  return LOG_INFO("TODO");
}

// just updates this chapter in the toc (fails if doesn't exist)
lob_t lobar_toc_mod(lob_t toc, lobar_t chapter)
{
  return LOG_INFO("TODO");
}

// removes any chapter w/ this title
lob_t lobar_toc_del(lob_t toc, char *title)
{
  return LOG_INFO("TODO");
}

// provide current chapter hash of the toc
uint32_t lobar_toc_hash(lob_t toc)
{
  return 0;
}
