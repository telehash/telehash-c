const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = require('joi');

const methodSchema = JOI.array().values(JOI.object.keys({
  name : JOI.string().min(1).required().label("Method Name").description("the key under which the function will be bound to the mesh or link")
  method : JOI.fun().required().label("Function").description("the function to bind to the mesh or link")
}).required().description("objects returned from ._linkMethods or ._meshMethods must conform")).

const LinkMethodSchema = methodSchema.label("Link.method() schema");
const MeshMethodSchema = methodSchema.label("Link.method() schema");

class TelehashUtilMiddleware extends TelehashAbstractMiddleware{
  constructor(){
    super();
  }

  get _type(){ return "util"; }

  get _schema(){
    return this.JOI.any().description("default options for abstract util");
  }

  get _enable(){
    this.log.info(`${this.name}._enable() called`);
    return this.validateMethods('link', LinkMethodSchema, this.linkMethods)
               .then((methods) => {
                 this.log.info(`${this.name}.linkMethods validated`);
                 this.mesh.on('link', this.linkListener);
                 this.log.info(`${this.name}.linkListener enabled`);
                 return this.attachMethods('link', this.mesh._links, methods);
               })
               .then(() => )
               .then(() => this.validateMethods(MeshMethodSchema, this.meshMethods))
               .then((methods) => {
                 this.log.info(`${this.name}.meshMethods validated`);
                 return this.attachMethods('mesh',[this.mesh], methods);
               })
               .then(() => this.log.info(`${this.name} mesh methods applied to mesh`))
  }

  attachMethods(type, iterable, methods){
    this.log.info(`${this.name} applying ${type} methods ${methods.map(({name}) => name).join(", ")}`)
    return Promise.resolve().then(() => {
      for (thing of iterable){
        for (({name, method}) of methods){
          if (!thing[name]) {
            thing[name] = method.bind(thing);
            thing[name].middleware = this.name;
            continue;
          } else {
            let warning = null;
            if (!thing[name].middleware){
              warning = `${this.name} util method ${type}.${name}() conflicting with existing method...\n\t\tEither pre-existing in telehash core or monkey patched outside of a middleware`;
            } else if(thing[name].middleware === this.name){
              warning = `${this.name} util method ${type}.${name}() conflicting with self`
            } else {
              warning = `${this.name} util method ${type}.${name}() conflicting with method of same name from ${thing[name].middleware}`
            }

            this.log.warn(warning);
            this.log.trace(warning);
          }
        }
      }
      this.log.info(`${this.name} ${type} methods applied to existing links`)
    })
  }

  detachMethods(type, iterable, methods){
    return
  }

  _disable(){
    this.log.info(`${this.name}._disable() called`);
    return this.validateMethods('link',LinkMethodSchema, this.linkMethods)
               .then((linkMethods) => {
                 this.mesh.removeListener(this.linkListener);

                 for (link of this.mesh._links){
                   for (({name}) of linkMethods){
                     delete link[name];
                   }
                 }

                 return this.validateMethods(MeshMethodSchema, this.meshMethods)
               })
               .then((meshMethods) => {
                 for (link of this.mesh._links){
                   for (({name}) of meshMethods){
                     delete this.mesh[name];
                   }
                 }
               })
               .then(() => {
                 this.log.info(`disabled ${this.name} util`);
               })
  }

  linkListener(link){
    if (!link) return log.error(new Error(`${this.name} util linkListener received no link`));
    this.log.info(`${this.name}.linkListener() called for ${link.id}`)

    this.validateMethods('link', LinkMethodSchema, this.linkMethods)
        .then((linkMethods) => this.attachMethods("link", [link], linkMethods))
  }

  validateMethods(type, schema, methods){
    this.log.info(`validating ${this.name} ${type} methods`);
    return TelehashAbstractMiddleware.validateSchemas(schema, methods).catch(e => {
      this.log.error(`failed to validate ${this.name} ${type} methods:`);
      this.log.error(e);
      this.emit('error',e);
      return Promise.reject(e);
    })
    .then((methods) => {
      this.log.info(`${this.name} ${type} methods validated`);
      return methods;
    });
  }

  get linkMethods(){
    let methods = this._linkMethods;
    if (typeof methods[Symbol.iterator] === 'function') return methods;

    let error = new Error(`${this.name}._linkMethods is not an array or iterable`);
    this.log.error(error);
    this.emit('error', error);
    return [];
  }

  get meshMethods(){
    let methods = this._meshMethods;
    if (typeof methods[Symbol.iterator] === 'function') return methods;

    let error = new Error(`${this.name}._linkMethods is not an array or iterable`);
    this.log.error(error);
    this.emit('error', error);
    return [];
  }

  get _linkMethods (){
    this.log.warn(`${this.name}._linkMethods undefined\n\t*** to silence this warning, implement a getter returning and empty array ***`);
    this.log.trace(`${this.name}._linkMethods undefined`);
    return [];
  }

  get _meshMethods (){
    this.log.warn(`${this.name}._meshMethods undefined\n\t*** to silence this warning, implement a getter returning and empty array ***`);
    this.log.trace(`${this.name}._meshMethods undefined`);
    return [];
  }
}
