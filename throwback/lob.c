/*
 LOB; // is singleton
 var lob = LOB.create(); // typeof "lob"
 lob.key = value; // lob setter
 lob.foo; // lob getter, creates copy of value
 LOB.body(lob); // returns body
 */

#include <dom.h>
#include <dew.h>

#define TYPEOF_LOB 6

static dew_t dom_lob_get(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  dew_set_char(result, lob_get(this->value, key), lob_get_len(this->value, key));
  return stack;
}

static dew_t dom_lob_set(dew_t stack, dew_t this, char *key, uint8_t len, dew_t val, void *arg)
{
  char* json = dew_json(val);
  lob_set_raw(this->value,key, len, json, strlen(json));
  return stack;
}

static dew_t dom_lob_body(dew_t stack, dew_t args, dew_t result)
{
  dew_t lob_d = args;
  dew_t to_set = dew_arg(args,2,IS_QUOTED);
  if (lob_d == NULL || lob_d->len != TYPEOF_LOB){
    dew_error(result);
    return stack;
  }
  if (to_set != NULL) {
    lob_body(lob_d->value, (uint8_t*) dew_get_char(to_set), dew_get_len(to_set));
  }
  dew_set_char(result, (char*)lob_body_get(lob_d->value), lob_body_len(lob_d->value));
  return stack;
}

static dew_t dom_lob_free(dew_t this, void *arg)
{
  return NULL;
}

struct dew_type_struct lob_type = {&dom_lob_get, &dom_lob_set, &dom_lob_free, NULL, TYPEOF_LOB};

static dew_t lob_singleton_getter(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg);
static dew_t dom_lob_create(dew_t stack, dew_t args, dew_t result);

dew_t dom_lob(dew_t stack)
{
  dew_set_type(stack,&lob_type);
  stack = dew_act_singleton(stack, "LOB", lob_singleton_getter, NULL, NULL);
  return stack;
}

static dew_t lob_singleton_getter(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  if (strncmp(key,"create",len) == 0){
    dew_set_fun(result, &dom_lob_create);
  }

  if (strncmp(key,"body",len) == 0) {
    dew_set_fun(result, &dom_lob_body);
  }
  return stack;
}

static dew_t dom_lob_create(dew_t stack, dew_t args, dew_t result)
{
  dew_set_object(result, TYPEOF_LOB, lob_new());
  return stack;
}
