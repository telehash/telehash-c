const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI;

const methodSchema = JOI.array().items(JOI.object().keys({
  name : JOI.string().min(1).required().label("Method Name").description("the key under which the function will be bound to the mesh or link"),
  method : JOI.func().required().label("Function").description("the function to bind to the mesh or link")
}).description("objects returned from ._linkMethods or ._meshMethods must conform"));

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

  _enable(mesh){
    this.log.debug(`._enable() START`);
    this.mesh = mesh;

    return Promise.all([
      this.validateMethods('link', LinkMethodSchema, this.linkMethods),
      this.validateMethods('mesh'  ,MeshMethodSchema, this.meshMethods)
    ])
    .then(([linkMethods, meshMethods]) => {
      this.log.debug(`._enable() methods validated`);
      mesh.on('link',(link) => this.linkListener(link));
      this.log.debug(`._enable() linkListener attached`);
      return Promise.all([
        this.attachMethods( 'link',mesh._links,linkMethods ),
        this.attachMethods( 'mesh',[mesh],meshMethods)
      ])
    })
    .then(() => {
      this.log.debug(`._enable() SUCCESS`);
    })
    .catch(e => {
      this.log.debug(`._enable() FAILED`);
      return Promise.reject(e)
    })
  }

  get _optionsSchema(){
    return TelehashUtilMiddleware.optionsSchema;
  }

  attachMethods(type, iterable, methods){
    this.log.debug(`.attachMethods('${type}', [${iterable.length}], ['${methods.map(({name}) => name).join("', '")}']) START`);
    if (type != 'mesh' && type != 'link') return Promise.reject(new Error(`unsupported type, expected 'mesh' or 'link', got ${typs}`))

    return Promise.resolve().then(() => {
      for (var thing of iterable){
        for (let {name, method} of methods){
          if (!thing[name]) {
            thing[name] = method.bind(thing);
            thing[name].middleware = this.name;
            continue;
          } else {
            let warning = null;
            if (!thing[name].middleware){
              warning = ` util method ${type}.${name}() conflicts with existing method...\n\t\tEither pre-existing in telehash core or monkey patched outside of a middleware`;
            } else if(thing[name].middleware === this.name){
              warning = ` util method ${type}.${name}() conflicts with self`
            } else {
              warning = ` util method ${type}.${name}() conflicts with method of same name from ${thing[name].middleware}`
            }

            this.log.warn(warning);
            this.log.trace(warning);
          }
        }
      }
      this.log.debug(`.attachMethods('${type}', [${iterable.length}], ['${methods.map(({name}) => name).join("', '")}']) SUCCESS`);
    }).catch((e) => {
      this.log.debug(`.attachMethods('${type}', [${iterable.length}], ['${methods.map(({name}) => name).join("', '")}']) FAILURE`);
      return Promise.reject(e);
    })
  }

  detachMethods(type, iterable, methods){
    this.log.debug(`.detachMethods('${type}', [${iterable.length}], ['${methods.map(({name}) => name).join("', '")}']) START`);
    return Promise.resolve().then(() => {
      for (var thing of iterable){
        for (let {name} of methods){
          if (thing[name]) {
            delete thing[name];
          } else {
            this.log.warn(`.detachMethods did not find ${name} on ${type}`);
            this.log.trace(thing);
          }
        }
      }
      this.log.debug(`.detachMethods('${type}', [${iterable.length}], ['${methods.map(({name}) => name).join("', '")}']) SUCCESS`);
    }).catch((e) => {
      this.log.debug(`.detachMethods('${type}', [${iterable.length}], ['${methods.map(({name}) => name).join("', '")}']) FAILURE`);
      return Promise.reject(e);
    })
  }

  _disable(){
    this.log.debug(`._disable() START`);
    return Promise.all([
      this.validateMethods('link',LinkMethodSchema, this.linkMethods),
      this.validateMethods('mesh', MeshMethodSchema, this.meshMethods)
    ]).then(([linkMethods, meshMethods]) => {
      this.log.debug(`._disable() passed method check`);
      if (!this.mesh) return Promise.reject(new Error(`._disable() called without .mesh assigned`));
      return Promise.all([
        this.detachMethods('link',this.mesh._links,linkMethods),
        this.detachMethods('mesh',[this.mesh],meshMethods)
      ])
    })
    .then(() => {
      this.log.debug(`._disable() SUCCESS`);
    })
    .catch(e => {
      this.log.debug(`._disable() FAILURE`);
      return Promise.reject(e);
    })
  }

  linkListener(link){
    if (!link) return log.error(new Error(` util linkListener received no link`));
    this.log.debug(`.linkListener() called for ${link.id}`)

    this.attachMethods("link", [link], this.linkMethods)
  }

  validateMethods(type, schema, methods){
    this.log.debug(`.validateMethods(${type}, [schema], [${methods.length}])`);
    return TelehashAbstractMiddleware.validateSchema(schema, methods).catch(e => {
      this.log.error(`.validateMethods(${type}, [schema], [${methods.length}]) FAILED`);
      this.log.error(e);
      this.emit('error',e);
      return Promise.reject(e);
    })
    .then((methods) => {
      this.log.debug(`.validateMethods(${type}, [schema], [${methods.length}]) SUCCESS`);
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

module.exports = TelehashUtilMiddleware;
