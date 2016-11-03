const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI;

class AbstractUtil extends TelehashAbstractMiddleware{
  static get methodSchema () {
    return  JOI.array().items(
      JOI.object().keys({
        name : JOI.string().min(1).required().label("Method Name").description("the key under which the function will be bound to the mesh or link"),
        method : JOI.func().required().label("Function").description("the function to bind to the mesh or link"),
        schema : JOI.object().schema().required().label("JOI Schemas").description("an array of validators to be applied to the arguments of this method"),
        Test : JOI.func().required()
      }).description("objects returned from ._linkMethods or ._meshMethods must conform")
    ).required().min(0);
  }

  static get LinkMethodSchema () { return AbstractUtil.methodSchema.label("Link.method() schema"); }
  static get MeshMethodSchema () { return AbstractUtil.methodSchema.label("Mesh.method() schema"); }

  static Test(CHILD, OPTIONS){
    class UtilTest extends AbstractUtil {
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
      this.validateMethods('link', AbstractUtil.LinkMethodSchema, this.linkMethods),
      this.validateMethods('mesh', AbstractUtil.MeshMethodSchema, this.meshMethods)
    ])
    .then(([linkMethods, meshMethods]) => {
      this.log.debug(`._enable() methods validated`);
      mesh.on('link',(link) => this.linkListener(link));
      let l = mesh.listeners('link')
      l.unshift(l.pop());
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
    return AbstractUtil.optionsSchema;
  }

  static makeEvent(stage, details) {
    return ({stage, details})
  }

  static curryProgressCallback({start = 0, end = 100, stage}, onProgress) {
    //console.log("curry")
    var parentStage = stage;
    return ({stage, percent, details}) => {
      onProgress({
        stage : `${parentStage}.${stage}`,
        percent : Math.round(start + percent * ((end - start) / 100)),
        details
      })
    }
  }

  curryMethod({schema, method, thing, name})  {
    return (options = {}, onProgress = () => {}) => {
      onProgress = AbstractUtil.curryProgressCallback(
        {
          start : 0,
          end : 100,
          stage : name
        },
        onProgress
      )
      this.log.debug(`._${name}(${options},${onProgress}) START`);
      onProgress({stage : "validate", percent : 2});
      return AbstractUtil
            .validateSchema(schema, options)
            .then(() => {
              this.log.debug(`._${name}(${options},${onProgress}) SCHEMA VALIDATED`);
              onProgress({stage : "start", percent : 5})
              var promise;
              try{
                promise = method.apply(
                  thing,
                  [
                    options,
                    AbstractUtil.curryProgressCallback({
                      start : 5,
                      end : 95,
                      stage : "run"
                    }, onProgress)
                  ]
                );
              } catch(e){
                e.message = `method '${name}' threw error : ${e.message}`;
                return Promise.reject(e);
              }

              if (promise instanceof Promise) return promise;

              return Promise.reject(new Error(`util method '${name}' did not return a promise`))
            })
            .then((result) => {
              this.log.debug(`._${name}(${options},${onProgress}) FINISH`);
              onProgress({
                stage : "finish",
                percent : 100
              })
              return result;
            })
            .catch(error => {
              this.log.error(error);
              onProgress({
                stage : "error",
                error : error,
                percent : -1
              });
              return Promise.reject(error);
            })
    }
  }

  attachMethods(type, iterable, methods){
    this.log.debug(`.attachMethods('${type}', [${iterable.length || iterable.size}], ['${methods.map(({name}) => name).join("', '")}']) START`);
    if (type != 'mesh' && type != 'link') return Promise.reject(new Error(`unsupported type, expected 'mesh' or 'link', got ${typs}`))

    return (new Promise((resolve, reject) => {
      for (var thing of iterable){
        thing = thing.length === 2 ? thing[1] : thing; // handle iteration over a Map()
        for (let {name, schema, method} of methods){
          if (!thing[name]) {
            thing[name] = this.curryMethod({method, schema, name, thing}).bind(thing);
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
      this.log.debug(`.attachMethods('${type}', [${iterable.length || iterable.size}], ['${methods.map(({name}) => name).join("', '")}']) SUCCESS`);
      resolve()
    })).catch((e) => {
      this.log.debug(`.attachMethods('${type}', [${iterable.length || iterable.size}], ['${methods.map(({name}) => name).join("', '")}']) FAILURE`);
      return Promise.reject(e);
    })
  }

  detachMethods(type, iterable, methods){
    this.log.debug(`.detachMethods('${type}', [${iterable.length || iterable.size}], ['${methods.map(({name}) => name).join("', '")}']) START`);
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
      this.log.debug(`.detachMethods('${type}', [${iterable.length || iterable.size}], ['${methods.map(({name}) => name).join("', '")}']) SUCCESS`);
    }).catch((e) => {
      this.log.debug(`.detachMethods('${type}', [${iterable.length || iterable.size}], ['${methods.map(({name}) => name).join("', '")}']) FAILURE`);
      return Promise.reject(e);
    })
  }

  _disable(){
    this.log.debug(`._disable() START`);
    return Promise.all([
      this.validateMethods('link', AbstractUtil.LinkMethodSchema, this.linkMethods),
      this.validateMethods('mesh', AbstractUtil.MeshMethodSchema, this.meshMethods)
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
    AbstractUtil.Test().then((inst) => {
      inst.log.info("TEST COMPLETE", inst.runtime);

      process.exit();
    })
    .catch(e => {
      console.log("*** FAIL ***",e)
    })
  }
}catch(e){
  console.log("BAD FAIL",e);
}

module.exports = AbstractUtil;
