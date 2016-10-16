var TelehashAbstractMiddleware require("./abstract.js");

class TelehashTransportMiddleware extends TelehashAbstractMiddleWare {
  constructor(){
    super();
  }

  get type(){ return "transport"; }

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
                                     .catch((e) => {{
                                       this.emit('error',e);
                                       return Promise.reject(e);
                                     }})
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
                 } finally {
                   error = error || new Error(`${this.name}._disconnect() returned '${typeof promise}' instead of a promise.`);
                   this.log.error(error);
                   this.emit('error', error);
                   return Promise.reject(error);
                 }
               });
  }

  announce(){
    let promise = this._announce();
    if (promise && typeof promise.then === "function") return promise;
    else this.emit('error', new Error(`${this.name}._listen() returned '${typeof promise}' instead of a promise.`));
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
