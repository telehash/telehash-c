// "XForm" singleton
// var x = XForm.create("type",...);
// var y = x.update(bytes);
// var z = x.final(bytes);
// var b64 = XForm.once("type",bytes); // only some transforms support this

static dew_t XForm_create(dew_t stack, dew_t args, dew_t result)
{
//  dew_t xforms = dew_get_this(stack);
  // TODO
  return stack;
}

static dew_t XForm_once(dew_t stack, dew_t args, dew_t result)
{
  // TODO
  return stack;
}

static dew_t XForm_getter(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *xforms)
{
  stack = dew_set_this(stack,(dew_t)xforms);
  if (strncmp(key,"create",len) == 0){
    dew_set_fun(result, &XForm_create);
  } else if (strncmp(key,"once",len) == 0) {
    dew_set_fun(result, &XForm_once);
  }else{
    dew_err(dew_set_char(result,"undefined",0));
  }
  return stack;
}

static dew_t tb_xform_get(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  // TODO
  return stack;
}


static dew_t tb_xform_free(dew_t this, void *arg)
{
  // TODO
  return this;
}

struct dew_type_struct tb_xform_type = {&tb_xform_get, NULL, &tb_xform_free, NULL, TYPEOF_XFORM};

dew_t dew_lib_xform(dew_t stack)
{
  dew_set_type(stack,&tb_xform_type);
  dew_t xforms = dew_new(); // internal stack where transform generators are stored, TODO need singleton free flow
  stack = dew_act_singleton(stack, "XForm", XForm_getter, NULL, xforms);
  return stack;
}

