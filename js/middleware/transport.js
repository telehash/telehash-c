const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI

class TelehashTransportMiddleware extends TelehashAbstractMiddleware {
  static Test(CHILD, OPTIONS){
    class TransportTest extends TelehashTransportMiddleware{
      constructor(){
        super();
      }

      get _name(){ return "TransportTest"; }

      get _environment(){
        return ({ internet : false, gps : false, runtime : ["any"], platform : ["any"] });
      }

      _listen(){
        return Promise.resolve();
      }

      _disable(){
        return Promise.resolve();
      }

      _connect(){
        return Promise.resolve();
      }

      _announce(){
        return Promise.resolve();
      }

      get _handleSchema(){
        return JOI.object({})
      }
    }

    TelehashAbstractMiddleware.Test(CHILD || TransportTest, OPTIONS)
                              .then((instance) => {
                                instance.log.info("TransportTest");
                                return instance;
                              })

  }

  static get optionsSchema(){
    return TelehashAbstractMiddleware.optionsSchema.concat(JOI.object())
  }

  constructor(){
    super();
  }

  get _optionsSchema(){
    return TelehashTransportMiddleware.optionsSchema;
  }

  get _type(){ return "transport"; }

  get handleSchema(){
    let schema = this._handleSchema;
    if (schema && schema.isJoi) return TelehashAbstractMiddleware.annotateSchema(this, schema)

    this.emit('error', new Error(`${this.name}._handleSchema is not a JOI object`));
    return this.JOI.any().valid(null).label("null").description("no value is accepted because an invalid schema was provided");
  }

  get _handleSchema(){
    this.log.warn(`${this.name}._handleSchema undefined`);
    this.log.trace(`${this.name}._handleSchema undefined`);
    return this.JOI.object();
  }

  get _enable() { return this.listen; }

  listen(options){
    let promise = this._listen(options);
    if (promise && typeof promise.then === "function") return promise;
    else this.emit('error', new Error(`${this.name}._listen() returned '${typeof promise}' instead of a promise.`));
  }

  validateHandle(handle){
    return TelehashAbstractMiddleware.validateSchema(this.handleSchema, handle)
                                     .catch((e) => {
                                       this.log.warn(e);
                                       return Promise.reject(e);
                                     })
  }

  connect(handle){
    return this.validateHandle(handle)
               .then(() => {
                 var error;
                 try{
                   let promise = this._handleEvent(event);
                   if (promise instanceof Promise) return promise;
                 } catch (e) {
                   error = e;
                 } finally {
                   error = error || new Error(`${this.name}._connect() returned '${typeof promise}' instead of a promise.`);
                   this.log.error(error);
                   this.emit('error', error);
                   return Promise.reject(error);
                 }
               })
  }

  disconnect(handle){
    return this.validateHandle(options)
               .then(() => {
                 var error;
                 try {
                   let promise = this._handleEvent(event);
                   if (promise instanceof Promise) return promise;
                 } catch (e) {
                   error = e;
                 }
                 error = error || new Error(`${this.name}._disconnect() returned '${typeof promise}' instead of a promise.`);
                 this.log.error(error);
                 this.emit('error', error);
                 return Promise.reject(error);

               });
  }

  announce(){
    var error;
    try {
      let promise = this._announce();
      if (promise instanceof Promise) return promise;
    } catch (e) {
      error = e;
    }
    error = error || new Error(`${this.name}._announce() returned '${typeof promise}' instead of a promise.`);
    this.log.error(error);
    this.emit('error', error);
    return Promise.reject(error);

  }

  _listen(){
    this.log.warn(`${this.name}._listen() undefined`);
    this.log.trace(`${this.name}._listen() undefined`);
    return Promise.reject(new Error(`${this.name}._listen() undefined`));
  }

  _connect(){
    this.log.warn(`${this.name}._connect() undefined`);
    this.log.trace(`${this.name}._connect() undefined`);
    return Promise.reject(new Error(`${this.name}._connect() undefined`));
  }

  _disconnect(){
    this.log.warn(`${this.name}._connect() undefined`);
    this.log.trace(`${this.name}._connect() undefined`);
    return Promise.reject(new Error(`${this.name}._connect() undefined`));
  }

  _announce(){
    this.log.warn(`${this.name}._announce() undefined`);
    this.log.trace(`${this.name}._announce() undefined`);
    return Promise.reject(new Error(`${this.name}._announce() undefined`));
  }
}

try{
  if (require.main === module) {
    console.log('called directly');
    TelehashTransportMiddleware.Test().then((env) => {
      console.log('passed Transport self test');
      process.exit();
    })
  }
}catch(e){}

module.exports = TelehashTransportMiddleware;
