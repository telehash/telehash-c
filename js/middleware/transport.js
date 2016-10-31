'use strict';
const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI
const THC = require("../core.js");
const Duplex = require("stream").Duplex;
const stringify = require('json-stringify-safe');

class TelehashTransportMiddleware extends TelehashAbstractMiddleware {
  static get Stream (){
    return Duplex;
  }

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
    }

    return TelehashAbstractMiddleware
          .Test(CHILD || TransportTest, OPTIONS)
          .then((transport) => new Promise((resolve, reject) => {
            var done = false;
            transport.log.info("START DISCOVERY TEST");
            transport.on('discover',(handle) => {
              transport.log.info('discover triggered', handle.type);
              done = true;
              transport.disable().then(() => resolve(transport)).catch(reject);
            })

            transport.enable(new THC.Mesh(() => {})).catch(reject);
            setTimeout(() => {
              if (!done) reject(new Error('timeout 10 seconds looking for device, is one available?'))
            },60000)
          }))
          .then((transport) => new Promise((resolve, reject) =>{
            transport.log.info("FULL LINK TEST START")
            var mesh = new THC.Mesh(() => {});
            mesh.use(transport);
            mesh.on('error', reject);
            transport.on('error',(e) => {
              transport.log.warn("transport error",e);
              reject(e)
            });

            mesh.start(true);

            return Promise.all([
              new Promise((resolve, reject) => {
                mesh.on('discover',(handle) => {
                  transport.log.debug('got discover', handle);
                  mesh.connect(handle)
                      .then(resolve)
                      .catch((e) => {
                        transport.log.warn("connect reject",e)
                        reject(e)
                      });
                })
              })
              , new Promise((resolve, reject) => {
                mesh.on('link',(link) => {
                  transport.log.info("got link from mesh");
                  resolve(transport);
                })
              })
            ]).then(resolve).catch(reject)
          }))
          .then(([link, transport]) => {
            return link.disconnect().then(() => transport)
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
    if (schema && schema.isJoi) return TelehashAbstractMiddleware.annotateSchema(schema);

    this.emit('error', new Error(`${this.name}._handleSchema is not a JOI object`));
    return this.JOI.any().valid(null).label("null").description("no value is accepted because an invalid schema was provided");
  }

  get _handleSchema(){
    this.log.warn(`${this.name}._handleSchema undefined`);
    this.log.trace(`${this.name}._handleSchema undefined`);
    return this.JOI.object();
  }

  listen(options){
    return this.enable(options);
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
               .then((handle) => {
                 this.log.debug(`.connect(${stringify(handle)})`)
                 var error, promise;
                 try{
                   promise = this._connect(handle);
                   if (promise instanceof Promise) return promise;
                 } catch (e) {
                   error = e;
                 }

                 error = error || new Error(`${this.name}._connect() returned '${typeof promise}' instead of a promise.`);
                 this.log.error(error);
                 this.emit('error', error);
                 return Promise.reject(error);

               })
  }

  disconnect(handle, serial){
    return this.validateHandle(handle)
               .then((handle) => {
                 var error;
                 try {
                   let promise = this._disconnect(handle, serial);
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

  _connect(){
    this.log.warn(`${this.name}._connect() undefined`);
    this.log.trace(`${this.name}._connect() undefined`);
    return Promise.reject(new Error(`${this.name}._connect() undefined`));
  }

  _disconnect(){
    this.log.warn(`${this.name}._disconnect() undefined`);
    this.log.trace(`${this.name}._disconnect() undefined`);
    return Promise.reject(new Error(`${this.name}._disconnect() undefined`));
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
