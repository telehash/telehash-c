/*
LOB; // is singleton
var lob = LOB.create(); // typeof "lob"
lob.key = value; // lob setter
lob.foo; // lob getter, creates copy of value
LOB.body(lob); // returns body
*/

#include "lob.h"
#define TYPEOF_LOB TYPOF_EXT1

struct dew_type_struct lob_type = {&lob_get, &lob_set, &lob_free, NULL, TYPEOF_LOB};

dew_t throwback_lib_lob(dew_t stack)
{
  dew_set_type(stack,TYPEOF_LOB);
  stack = dew_act_singleton(stack, "LOB", lob_singleton_getter, NULL, NULL);
  return stack;
}

static dew_t lob_singleton_getter(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  if (strcmp(key,"create") == 0){
    dew_set_fun(result, &lob_create);
  } else if (strcmp(key,"body") == 0) {
    dew_set_fun(result, &lob_body);
  }
}

static dew_t lob_create(dew_t stack, dew_t args, dew_t result)
{
  dew_set_object(result, TYPEOF_LOB, lob_new());
  return stack;
}

static dew_t lob_get(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  dew_set_char(result, lob_get(this->value, key), lob_get_len(this->value, key));
  return stack;
}

static dew_t lob_set(dew_t stack, dew_t this, char *key, uint8_t len, dew_t val, void *arg)
{
  lob_set_len(this->value, dew_get_char(val), dew_get_len(val));
  return stack;
}

static dew_t lob_body(dew_t stack, dew_t args, dew_t result)
{
  dew_t lob_d = dew_arg(args, 0, IS_TYPED);
  dew_t to_set = dew_arg(args, 1, IS_QUOTED);
  if (lob_d == NULL || lob_d->len != TYPEOF_LOB){
    dew_error(result, "illegal invocation, use LOB.body(<lob>, [string])");
    return stack;
  }
  if (to_set != NULL) {
    lob_set_len(lob_d->value, dew_get_char(to_set), dew_get_len(to_set));
  }
  dew_set_char(result, lob_body_get(lob_d->value), lob_body_len(lob_d->value));
  return stack;
}

static dew_t lob_free(dew_t this, void *arg)
{

}
