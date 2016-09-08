// "XForm" singleton
// var x = XForm.create("type",...);
// var y = x.update(bytes);
// var z = x.final(bytes);
// var b64 = XForm.base64(bytes); // only some transforms support this

static dew_t XForm_create(dew_t stack, dew_t args, dew_t result)
{
  dew_t xforms = dew_get_this(stack);
  dew_t xkey;
  if(!(xkey = dew_get(xforms,dew_get_char(args),dew_get_len(args))) || !dew_typeof(xkey->statement,TYPEOF_SINGLETON)) return dew_error(dew_set_char(result,"unsupported",0), stack);

  // get the actual create method
  dew_type_t xform = (dew_type_t)(xkey->statement->value);
  dew_fun_t create = (dew_fun_t)xform->getter;

  // a blank state for create to fill in
  dew_t state = dew_new();
  state->block = xkey->statement; // attach implementation context

  // run create and safely catch if it errored
  if((create && !create(stack, args, state)) || dew_get_err(state))
  {
    dew_clone(result,state);
    state->block = NULL; // detach
    dew_free(state);
    return stack;
  }
  
  // have a state, wrap it w/ TYPEOF_XFORM
  dew_set_object(result,TYPEOF_XFORM,state);
  
  return stack;
}

static dew_t XForm_once(dew_t stack, dew_t args, dew_t result)
{
  dew_t xf = dew_get_this(stack);
  if(!dew_typeof(xf,TYPEOF_SINGLETON)) return dew_error(dew_set_char(result,"invalid usage",0), stack);
  dew_type_t xform = (dew_type_t)(xf->value);
  dew_fun_t create = (dew_fun_t)xform->getter;
  dew_fun2_t process = (dew_fun2_t)xform->setter;

  dew_t state = dew_new();
  if(create) create(stack, args, state);
  process(stack, state, args, NULL, result);
  process(stack, state, NULL, args, result);
  if(xform->free) xform->free(state, NULL);
  dew_free(state);
  
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
    stack = dew_set_this(stack, xkey);
    dew_set_fun(result, &XForm_once);
  }else{
    dew_err(dew_set_char(result,"undefined",0));
  }
  return stack;
}

static dew_t xform_update(dew_t stack, dew_t args, dew_t result)
{
  dew_t state = dew_get_this(stack);
  if(!dew_typeof(state,TYPEOF_XFORM) || !dew_typeof(state->block,TYPEOF_SINGLETON)) return dew_error(dew_set_char(result,"invalid usage",0), stack);
  dew_type_t xform = (dew_type_t)(state->block->value);
  dew_fun2_t process = (dew_fun2_t)xform->setter;

  // run process w/ update arg
  if(!process(stack, state, args, NULL, result)) return dew_error(dew_set_char(result,"failed",0), stack);

  return stack;
}

static dew_t xform_final(dew_t stack, dew_t args, dew_t result)
{
  dew_t state = dew_get_this(stack);
  if(!dew_typeof(state,TYPEOF_XFORM) || !dew_typeof(state->block,TYPEOF_SINGLETON)) return dew_error(dew_set_char(result,"invalid usage",0), stack);
  dew_type_t xform = (dew_type_t)(state->block->value);
  dew_fun2_t process = (dew_fun2_t)xform->setter;

  // run process w/ final arg
  if(!process(stack, state, NULL, args, result)) return dew_error(dew_set_char(result,"failed",0), stack);
  
  return stack;
}

static dew_t xform_getter(dew_t stack, dew_t this, char *key, uint8_t len, dew_t result, void *arg)
{
  // set this to the state object
  stack = dew_set_this(stack, this->value);
  if(strncmp(key,"update",len) == 0){
    dew_set_fun(result, &xform_update);
  }else if(strncmp(key,"final",len) == 0){
    dew_set_fun(result, &xform_final);
  }else{
    dew_err(dew_set_char(result,"undefined",0));
  }
  return stack;
}


static dew_t xform_free(dew_t this, void *arg)
{
  if(!this->value) return this;
  dew_t state = (dew_t)(this->value);
  if(!state->block || !state->block->value) return this;

  dew_type_t xform = (dew_type_t)(state->block->value);
  if(xform->free) xform->free(state, arg);

  state->block = NULL;
  this->value = dew_free(state);

  return this;
}

struct dew_type_struct tb_xform_type = {&xform_getter, NULL, &xform_free, NULL, TYPEOF_XFORM};

dew_t dew_lib_xform(dew_t stack)
{
  dew_set_type(stack,&tb_xform_type);
  dew_t xforms = dew_new(); // internal stack where transform generators are stored, TODO need singleton free flow
  stack = dew_act_singleton(stack, "XForm", XForm_getter, NULL, xforms);
  return stack;
}

// similar to singleton pattern, registers a transform
// NOTE: create and process cannot modify stack, process args1/2 swap for update vs final calls
dew_t dew_set_xform(dew_t stack, char *name, dew_fun_t create, dew_fun2_t process, dew_free_t free, void 
  *arg)
{
  dew_t d = dew_get(stack, "XForm", 5);
  dew_type_t XForm;
  dew_t xforms;
  if(!d || !(XForm = d->value) || !(xforms = XForm->arg)) return LOG_WARN("XForm missing/bad");

  // create the local storage for it, overloading type methods to our own needs
  dew_type_t xform  = malloc(sizeof(struct dew_type_struct));
  memset(xform,0,sizeof(struct dew_type_struct));
  xform->getter = (dew_getter_t)create;
  xform->setter = (dew_setter_t)process;
  xform->free = free;
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
