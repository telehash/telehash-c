
static dew_t hex_process(dew_t stack, dew_t act, dew_t args1, dew_t args2, dew_t result)
{
  if(!args1) return stack;
  if(args1->of != OF_CHAR) return dew_error(dew_set_char(result,"invalid arg",0), stack);
  uint32_t len = dew_get_len(args1) * 2;
  char *hex = malloc(len);
  util_hex((uint8_t*)dew_get_char(args1),dew_get_len(args1),hex);
  dew_set_char(result,hex,len);
  result->local = 1;
  return stack;
}

static dew_t unhex_process(dew_t stack, dew_t act, dew_t args1, dew_t args2, dew_t result)
{
  if(!args1) return stack;
  if(args1->of != OF_CHAR) return dew_error(dew_set_char(result,"invalid arg",0), stack);
  uint32_t len = dew_get_len(args1) / 2;
  uint8_t *bin = malloc(len);
  util_unhex(dew_get_char(args1),dew_get_len(args1),bin);
  dew_set_char(result,(char*)bin,len);
  result->is = IS_BUFFER;
  result->local = 1;
  return stack;
}

dew_t dew_lib_xform_hex(dew_t stack)
{
  stack = dew_set_xform(stack, "hex", NULL, hex_process, NULL, NULL);
  stack = dew_set_xform(stack, "unhex", NULL, unhex_process, NULL, NULL);
  return stack;
}

