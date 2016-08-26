#include "dew.h"
#include "../test/unit_test.h"

#define TYPEOF_LOB TYPEOF_EXT1
extern dew_t telehash_dew(dew_t stack, bool mesh);

int main(int argc, char **argv)
{
  dew_t stack = dew_lib_js10(NULL);
  fail_unless(stack);
  stack = telehash_dew(stack, true);
  fail_unless(stack);
  
  dew_t res = dew_new();
  stack = dew_eval(stack, dew_set_char(dew_new(),"lob = LOB.create()",0), res);
  fail_unless(stack);
  fail_unless(dew_typeof(res, TYPEOF_LOB));

  stack = dew_eval(stack, dew_set_char(dew_new(),"LOB.body(lob)",0), res);
  fail_unless(dew_is(res, OF_CHAR, IS_QUOTED));
  fail_unless(dew_get_len(res) == 0);

  stack = dew_eval(stack, dew_set_char(dew_new(),"LOB.body(lob,'body')",0), res);
  fail_unless(dew_is(res, OF_CHAR, IS_QUOTED));
  fail_unless(dew_get_len(res) == 4);
  
  stack = dew_eval(stack, dew_set_char(dew_new(),"lob.foo",0), res);
  fail_unless(stack);
  fail_unless(dew_get_cmp(res,"undefined",0));
  dew_reset(res);

  stack = dew_eval(stack, dew_set_char(dew_new(),"lob.foo = 'bar';lob.foo",0), res);
  fail_unless(stack);
  dew_dump(res);
  fail_unless(dew_get_cmp(res,"bar",0));

  return 0;
}

