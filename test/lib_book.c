#include "lib.h"
#include "util.h"
#include "e3x.h"
#include "unit_test.h"

int main(int argc, char **argv)
{
  chapter_t ch = chapter_new("title",42,42,42);
  fail_unless(ch);
  chapter_free(ch);
  
  chapter_t ch1 = chapter_new("one",1,0,0);
  lob_t ch1_val = lob_set(lob_new(),"howdy","partner");
  fail_unless(chapter_set(ch1, ch1_val));
  fail_unless(chapter_len(ch1) == lob_len(ch1_val));

  LOG_DEBUG("hash is %u",chapter_hash(ch1));
  fail_unless(chapter_hash(ch1) == 20068273);
  fail_unless(chapter_pager(ch1,1,lob_raw(ch1_val),lob_len(ch1_val)) == 0);
  fail_unless(chapter_verify(ch1,false));

  char *ch1_json = lob_json(chapter_json(ch1));
  LOG_DEBUG("chapter: %s",ch1_json);
  fail_unless(strcmp(ch1_json,"{\"title\":\"one\",\"start\":1,\"len\":21,\"hash\":20068273}") == 0);

  char *hex = util_hex(chapter_index(ch1),16,NULL);
  fail_unless(hex);
  LOG_DEBUG("hex of index page: %s",hex);
  fail_unless(strcmp(hex,"b13732011500000001006f6e65000000") == 0);
  fail_unless(chapter_head(ch1,lob_raw(ch1_val)) == 1+2); // only 2 pages long
  
  // create a book and test it
  lob_t toc = book_add(NULL, ch1);
  fail_unless(toc);
  fail_unless(book_chapters(toc) == 1);
  fail_unless(book_get(toc,"one"));
  
  char *toc_json = lob_json(book_json(toc));
  fail_unless(toc_json);
  LOG_DEBUG("book: %s",toc_json);
  fail_unless(strcmp(toc_json,"{\"one\":[1,2,21,20068273]}") == 0);
  
  fail_unless(book_del(toc,"one"));
  fail_unless(book_chapters(toc) == 0);

  return 0;
}

