const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI;

const methodSchema = JOI.array().items(JOI.object().keys({
  name : JOI.string().min(1).required().label("Method Name").description("the key under which the function will be bound to the mesh or link"),
  method : JOI.func().required().label("Function").description("the function to bind to the mesh or link")
}).required().description("objects returned from ._linkMethods or ._meshMethods must conform"));

const LinkMethodSchema = methodSchema.label("Link.method() schema");
const MeshMethodSchema = methodSchema.label("Mesh.method() schema");

class TelehashUtilMiddleware extends TelehashAbstractMiddleware{
  static Test(CHILD, OPTIONS){
    class UtilTest extends TelehashUtilMiddleware{
      constructor(){
        super()
      }

      get _name() { return "UtilTest";}

      get _environment() {return {internet : false, gps : false, runtime : ['any'], platform : ['any']}}

      get _linkMethods(){
        return ([{name : "script", method : (cmd) => {}/* this.channel({json : cmd})...*/}])
      }

      get _meshMethods(){
        return ([{name : "linkpool", method : () => {}}])
      }

    }

    return TelehashAbstractMiddleware.Test(CHILD || UtilTest, OPTIONS)
                                     .then((instance) => {
                                       instance.log.info("UtilTest completed");
                                       return instance;
                                     })
  }

  static get optionsSchema(){
    return TelehashAbstractMiddleware.optionsSchema.concat(JOI.object());
  }

  constructor(){
    super();
  }

  get _type(){ return "util"; }

  _enable(){
    this.log.info(`._enable() called`);
    return this.validateMethods('link', LinkMethodSchema, this.linkMethods)
               .then((methods) => {
                 this.log.info(`.linkMethods validated`);
                 this.mesh.on('link', this.linkListener);
                 this.log.info(`.linkListener enabled`);
                 return this.attachMethods('link', this.mesh._links, methods);
               })
               .then(() => this.validateMethods(MeshMethodSchema, this.meshMethods))
               .then((methods) => {
                 this.log.info(`.meshMethods validated`);
                 return this.attachMethods('mesh',[this.mesh], methods);
               })
               .then(() => this.log.info(` mesh methods applied to mesh`))
  }

  get _optionsSchema(){
    return TelehashUtilMiddleware.optionsSchema;
  }

  attachMethods(type, iterable, methods){
    this.log.info(` applying ${type} methods ${methods.map(({name}) => name).join(", ")}`)
    return Promise.resolve().then(() => {
      for (thing of iterable){
        for ({name, method} of methods){
          if (!thing[name]) {
            thing[name] = method.bind(thing);
            thing[name].middleware = this.name;
            continue;
          } else {
            let warning = null;
            if (!thing[name].middleware){
              warning = ` util method ${type}.${name}() conflicting with existing method...\n\t\tEither pre-existing in telehash core or monkey patched outside of a middleware`;
            } else if(thing[name].middleware === this.name){
              warning = ` util method ${type}.${name}() conflicting with self`
            } else {
              warning = ` util method ${type}.${name}() conflicting with method of same name from ${thing[name].middleware}`
            }

            this.log.warn(warning);
            this.log.trace(warning);
          }
        }
      }
      this.log.info(` ${type} methods applied to existing links`)
    })
  }

  detachMethods(type, iterable, methods){
    return
  }

  _disable(){
    this.log.info(`._disable() called`);
    return this.validateMethods('link',LinkMethodSchema, this.linkMethods)
               .then((linkMethods) => {
                 this.mesh.removeListener(this.linkListener);

                 for (link of this.mesh._links){
                   for ({name} of linkMethods){
                     delete link[name];
                   }
                 }

                 return this.validateMethods(MeshMethodSchema, this.meshMethods)
               })
               .then((meshMethods) => {
                 for (link of this.mesh._links){
                   for ({name} of meshMethods){
                     delete this.mesh[name];
                   }
                 }
               })
               .then(() => {
                 this.log.info(`disabled`);
               })
  }

  linkListener(link){
    if (!link) return log.error(new Error(` util linkListener received no link`));
    this.log.info(`.linkListener() called for ${link.id}`)

    this.validateMethods('link', LinkMethodSchema, this.linkMethods)
        .then((linkMethods) => this.attachMethods("link", [link], linkMethods))
  }

  validateMethods(type, schema, methods){
    this.log.info(`validating ${type} methods`);
    return TelehashAbstractMiddleware.validateSchema(schema, methods).catch(e => {
      this.log.error(`failed to validate  ${type} methods:`);
      this.log.error(e);
      this.emit('error',e);
      return Promise.reject(e);
    })
    .then((methods) => {
      this.log.info(` ${type} methods validated`);
      return methods;
    });
  }

  get linkMethods(){
    let methods = this._linkMethods;
    if (typeof methods[Symbol.iterator] === 'function') return methods;

    let error = new Error(`._linkMethods is not an array or iterable`);
    this.log.error(error);
    this.emit('error', error);
    return [];
  }

  get meshMethods(){
    let methods = this._meshMethods;
    if (typeof methods[Symbol.iterator] === 'function') return methods;

    let error = new Error(`._linkMethods is not an array or iterable`);
    this.log.error(error);
    this.emit('error', error);
    return [];
  }

  get _linkMethods (){
    this.log.warn(`._linkMethods undefined\n\t*** to silence this warning, implement a getter returning an empty array ***`);
    this.log.trace(`._linkMethods undefined`);
    return [];
  }

  get _meshMethods (){
    this.log.warn(`._meshMethods undefined\n\t*** to silence this warning, implement a getter returning an empty array ***`);
    this.log.trace(`._meshMethods undefined`);
    return [];
  }
}

try{
  if (require.main === module) {
    console.log('called directly');
    TelehashUtilMiddleware.Test().then((inst) => {
      inst.log.info("TEST COMPLETE", inst.runtime);

      process.exit();
    })
  }
}catch(e){}

module.exports = TelehashAbstractMiddleware;
