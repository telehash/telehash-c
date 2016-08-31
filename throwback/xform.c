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
  dew_t xf = dew_get_this(stack);
  dew_type_t xform = (dew_type_t)xf;
  dew_fun_t create = (dew_fun_t)xform->getter;
  dew_fun_t update = (dew_fun_t)xform->setter;
  dew_fun_t final = (dew_fun_t)xform->free;

  // TODO WIP
  stack = create(stack, args, result);
  stack = update(stack, args, result);
  stack = final(stack, args, result);
  
  return stack;
}

static dew_t XForm_getter(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  dew_t xforms = (dew_t)arg;
  dew_t xkey = NULL;
  if(strncmp(key,"create",len) == 0){
    stack = dew_set_this(stack, xforms);
    dew_set_fun(result, &XForm_create);
  }else if((xkey = dew_get(xforms,key,len))){
    stack = dew_set_this(stack, xkey->statement);
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

// similar to singleton pattern, registers a transform
dew_t dew_set_xform(dew_t stack, char *name, dew_fun_t create, dew_fun_t update, dew_fun_t final, void 
  *arg)
{
  dew_t d = dew_get(stack, name, 0);
  dew_type_t XForm;
  dew_t xforms;
  if(!d || !(XForm = d->value) || !(xforms = XForm->arg)) return LOG_WARN("XForm missing/bad");

  // create the local storage for it, overloading type methods to our own needs
  dew_type_t xform  = malloc(sizeof(struct dew_type_struct));
  memset(xform,0,sizeof(struct dew_type_struct));
  xform->getter = (dew_getter_t)create;
  xform->setter = (dew_setter_t)update;
  xform->free = (dew_free_t)final;
  xform->arg = arg;

  // register on stack
  xforms = dew_tush(xforms,OF_CHAR);
  xforms->value = strdup(name);
  xforms->local = 1;
  xforms->len = strlen(name);
  xforms->is = IS_BARE;
  xforms->statement = dew_new();
  dew_set_object(xforms->statement,TYPEOF_SINGLETON,xform);
  XForm->arg = xforms;
  
  return stack; // unchanged
}
