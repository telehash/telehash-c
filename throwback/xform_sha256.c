
static dew_t _create(dew_t stack, dew_t args, dew_t result)
{
  
}

static dew_t _process(dew_t stack, dew_t act, dew_t args1, dew_t args2, dew_t result)
{
  
}

static dew_t _free(dew_t d, void *arg)
{
  
}

dew_t xform_(dew_t stack)
{
  stack = dew_set_xform(stack, "foo", _create, _process, _free, NULL);
  return stack;
}
