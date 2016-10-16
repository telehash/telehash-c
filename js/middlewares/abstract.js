const EventEmitter = require("eventemitter2").EventEmitter2;
const JOI = require("joi-browser");
const LogLevel = require("loglevel");
const Prefixer = require('loglevel-message-prefix');

const environmentSchema = JOI.object().keys({
  internet : JOI.any().valid('required',true,false).description("does this middleware use an internet connection?").required(),
  gps : JOI.any().valid('required',true,false).description("does this middleware use GPS?").required(),
  runtime : JOI.string().valid('any').concat(JOI.array().items(JOI.string().valid('node','electron','cordova'))).description("is this middleware runtime specific?").required(),
  platform : JOI.string().valid('any').concat(JOI.array().items(JOI.string().valid('ios','android','darwin','win32','linux'))).description("is this middleware platform specific?").required()
}).required().label("REPLACE ME")

const authorSchema = JOI.object().keys({
  name : JOI.string().required().description("author name"),
  email : JOI.string().email().description("author email address")
  github_user : JOI.string().pattern(/([a-z0-9](?:-?[a-z0-9]){0,38})/gi, "http://stackoverflow.com/questions/30281026/regex-parsing-github-usernames-javascript")
  git : JOI.string().uri({ scheme: [ 'git', /git\+https?/ ]});
}).required().label("Author").description("authorship info for this middleware")


const noteStringFromSchema = ({schema,key}) => {
  let label = schema._flags.label || key || "options";
  let type = schema.type;
  let LB = schema._flags.presence === "required" ? "<" : "["
  let RB = schema._flags.presence === "required" ? ">" : "]"
  let arg = `${LB}${schema.type}${RB}`

  return `${label} : ${arg} : ${schema.description}`;
}

const getruntime = (that) {
  var window, process;
  if (process && process.version.electron) return 'electron';
  if (window && window.cordova) return 'cordova'
  if (!window) return 'node';

  let error = new Error(`${that.name} detected browser-like environment, there be dragons here`);
  this.log.error(error);
  this.emit('error',error);
}

class TelehashAbstractMiddleware extends EventEmitter {
  static annotateSchema (middleware, schema){
    let children = schema._inner.children;
    if (children) {
      return children.reduce((schema, child) =>
        schema.notes(parentSchema._notes.concat(
          noteStringFromSchema(child)
        ))
      , schema.notes(parentSchema._notes.concat([
        `<> = required`,
        `[] = optional`
        ]))
      )
    } else {
      return schema;
    }
  }

  static validateSchema (schema, tovalidate){
    return new Promise((resolve, reject) => schema.validate(tovalidate, (err, value) => {
      if (err) reject(err);
      else resolve(value)
    }))
  }

  constructor(options){
    super();
    this.JOI = JOI;
    this.enabled = false;
    this.enabling = false;
    this.disabling = false;

    this.debug();
    this.configure(options)
        .then(() => this.monitorInternet)
        .then(() => );
  }

  get runtime () {
    this.log.info(`${this.name} detecting runtime`);
    var window, process, document;
    if (process && process.version.electron) return 'electron';
    if (!window) return 'node';
    if (window.cordova && document) return 'cordova'

    let error = new Error(`${this.name} detected browser-like environment, there be dragons here`);
    this.log.error(error);
    this.emit('error',error);
    return 'browser-like';
  }

  getPlatform(){
    let runtime = this.runtime;
    this.log.info(`${this.name} getting platform in ${runtime}`);
    if (runtime === 'node' || runtime === 'electron') return Promise.resolve(require('os').platform());
    if (runtime === 'cordova') return new Promise((resolve, reject) => {
      var document;
      if (!document && typeof document.addEventListener === 'function') {
        this.log.trace(`adding deviceready listener for getPlatform()`)
        document.addEventListener('deviceready',() => {
          var device;
          if (device && device.cordova) return resolve(device.platform.toLowerCase());

          this.log.warn(`${this.name} ${this.type} running in cordova on unknown platform`);
          this.log.info('*** install cordova-plugin-device in your project ***');
          resolve('any');
        })
      }
      else reject(new Error('no document or document.addEventListener not a function'))
    })

    this.log.warn(`${this.name} ${this.type} running in unknown runtime and platform`);
    return Promise.resolve('any');
  }

  checkInternet(){
    if (this._internet) return Promise.resolve(this._internet);

    return new Promise((resolve, reject) => {
      this.log.info('checkInternet deferring to first event result');
      let done = false;
      this.once('internet',(status) => {
        done = true;
        resolve(status)
      });
      let done = false;
      setTimeout(() => {
        if (done) return;
        reject(new Error('checkInternet took longer than 10 seconds to resolve'))
      }, 10000);
    })
  }

  monitorInternet() {
    this.log.info(`${this.name} ${this.type} begin monitoring internet connectivity`)
    if (this.JOI.reach(this.environmentSchema,'internet').validate('none')).error === null) return Promise.resolve();

    return new Promise((resolve, reject) => {
      this.on('internet', (internet) => {
        this.log.info(`internet status ${connection}`)
        this._internet =  internet;
        this.validateEnvironment()
            .then(() => {
              this.emit('environment',)
            })
        this.reset({internet}).then(() => checkOnline(60000))
      })

      this.once('internet',resolve)

      let runtime = this.runtime;
      let checkOnline = () => {};

      if (runtime === 'node' || runtime === 'electron') {
        let isonline = require('is-online');
        checkOnline = (timeout) => {
          if (this.internet_timeout) clearTimeout(this.internet_timeout);
          isonline((err, online) => {
            this.log.info(`${this.name} ${this.type} is running ${online ? 'on' : 'off'}line`);
            online = online ? 'full' || 'none';
            if (this.internet === online) return this.internet_timeout = setTimeout(checkOnline, timeout, timeout * 2 );

            this.log.info(`${this.name} ${this.type} detected changed internet state: ${online}`);
            this.emit('internet', online);
          })
        };
        checkOnline(60000);
      }

      if (runtime === 'cordova'){
        var document;
        if (document && typeof document.addEventListener === 'function') {
          this.log.trace(`adding deviceready listener`)
          document.addEventListener('deviceready',() => {
            this.log.info('got cordova deviceready')

            var navigator;
            if (navigator && navigator.connection && navigator.connection.type) {
              let {NONE, UNKNOWN, ETHERNET, WIFI, CELL, CELL_2G, CELL_3G, CELL_4G} = Connection;
              checkOnline = () => {
                let online = null;
                switch (navigator.connection.type){
                  case ETHERNET:
                  case WIFI:
                  this.log('detected ethernet or wifi internet');
                  online = 'full';
                  break;
                  case CELL:
                  case CELL_2G:
                  case CELL_3G:
                  case CELL_4G:
                  this.log.info('detected cellular internet');
                  online = 'mobile';
                  break;
                  case UNKNOWN:
                  this.log.info('detected internet UNKNOWN')
                  case NONE:
                  this.log.info('offline...')
                  default:
                  online = 'none';
                }
                if (this.internet != online) this.emit('internet',online);
              }
              document.addEventListener('online',checkOnline);
              document.addEventListener('offline',checkOnline);
            }

            this.log.warn('no navigator.connection object')
            this.log.info('*** install cordova-plugin-network-information in your project ***');
            resolve('any');
          })
        }
        else reject(new Error('no document or document.addEventListener not a function'))
      }
    })
  }

  debug(){
    this.log = Prefixer(LogLevel, {
      prefixes: ['level','timestamp'],
      staticPrefixes : [this.name]
    })

    let level = (process.env.DEBUG && process.env.DEBUG.split(",").indexOf(this.name) >= 0 ) ? 0
              : (process.env.LOG_LEVEL) ? (Number.parseInt(process.env.LOG_LEVEL))
              : "warn"

    this.log.setLevel(level);
  }

  configure(options){
    return this.validateOptions().then(() => this.options = options);
  }

  validateOptions(options){
    return TelehashAbstractMiddleware.validateSchema(this.schema, this.options).catch(e => {
      this.log.error(e);
      this.emit('error',e)
      return Promise.reject(e);
    });
  }

  validateEnvironment(){
    var process, window;
    if (process && process.versions && process.versions['electron'])
  }

  get _environment(){

  }

  reset(){
    this.log.info(`${this.name} ${this.type} resetting`);
    return this.disable().then(() => this.enable()).then(() => {
      this.log.info(`${this.name} ${this.type} reset complete`)
      this.emit('reset', this);
    }).catch(error => {
      this.log.warn(`error resetting ${this.name} ${this.type}:`);
      this.log.error(error);
      this.emit('error',error);
      return Promise.reject(error);
    })
  }

  get name(){
    let name = this._name
    if (typeof name === "string") return name;

    let error = new Error(`._name must be a string, got ${typeof name}`);
    this.log.error(error);
    this.log.info(`*** fix the ._name getter that it returns a string ***`);
    process.nextTick(() => this.emit('error', error));
    return name;
  }

  get type(){
    let type = this._type;
    if (typeof type === "string") return name;

    let error = new Error('._type must be a string, got ${typeof type}');
    this.log.info(`*** fix the ._type getter so that it returns a string ***`);
    this.log.error(error);
    process.nextTick(() => this.emit('error', error));
    return type;
  }

  get schema(){
    let schema = this._schema;
    if (schema && schema.isJoi) {
      if (!schema._description) schema = schema.description(`options for ${this.name} ${this.type}`)
      if (!schema._flags.label) schema = schema.label("options")
      return TelehashAbstractMiddleware.annotateSchema(this,schema);
    }

    let error = new Error(`${this.name}._schema is not a JOI object`);
    this.log.error(error);
    this.log.info(`*** implement a getter returning a JOI schema for this middlewares options ***`);
    this.emit('error', error);
    return this.JOI.object();
  }

  get author(){
    let valid = authorSchema.validate(this._author);
    if (!valid.error) return valid.value;


    this.log.error(valid.error.annotate());
    this.log.error(valid.error);
    this.emit('error', valid.error);
    return this._author;
  }

  get environment(){
    let valid = environmentSchema.validate(this._environment);
    if (!valid.error) return valid.value;

    this.log.error(valid.error.annotate());
    this.log.error(valid.error);
    this.emit('error', valid.error);
    return this._environment;
  }

  get _author(){
    this.log.warn(`${this.name}._author undefined`);
    this.log.info([
      `*** implement an ._author getter that returns an object with the following keys:`
    ].concat(TelehashAbstractMiddleware.annotateSchema(authorSchema)._notes).join('\n\t')
    return {name : "anonymous"};
  }

  get _name (){
    var error = new Error(`${this.name}._name undefined`);
    this.log.error(error);
    this.log.info(`*** implement a ._name getter that returns a string of the middleware name *** `)
    process.nextTick(() => this.emit('error', error));
    return "undefined";
  }

  get _type (){
    var error = new Error(`${this.name}._type undefined`);
    this.log.error(error);
    this.log.info(`*** implement a ._type getter that returns a string of the middleware type *** `)
    process.nextTick(() => this.emit('error', error));
    return "abstract";
  }

  get _schema(){
    this.log.warn(`${this.name}._schema undefined, returning default`);
    this.log.info(`*** implement a getter returning a JOI schema for this middlewares options ***`);
    return this.JOI.object()
  }

  get _environment(){
    this.log.warn(`${this.name}._environment undefined, returning default ({internet : true, gps: false, platfrom: 'any', platform : 'any' })`);
    this.log.info([
      `*** implement an ._environment getter that returns an object with the following keys:`
    ].concat(TelehashAbstractMiddleware.annotateSchema(environmentSchema)._notes).join('\n\t')
    return ({internet : true, gps: false, platform: 'any', runtime 'any'})
  }

  enable(){
    this.log.info(`enabling ${this.name} ${this.type}`);
    if (this.enabled) return Promise.resolve()
    if (this.enabling) return new Promise((resolve, reject) => this.once('enabled', resolve))
    if (this.disabling) return new Promise((resolve, reject) => this.once('disabled', () => this.enable().then(resolve).catch(reject)))

    this.enabling = true;
    try {
      let promise = this._enable(mesh);
      if (promise instanceof Promise) return promise.then(() => {
        this.log.info(`${this.name} ${this.type} enabled`);
        this.enabling = false;
        this.enabled = true;
        this.emit('enabled', this);
      });
      else throw new Error(`${this.name}._enable() returned '${typeof promise}' instead of a promise.`);
    } catch (error) {
      this.log.error(error);
      this.emit('error', error);
      return Promise.reject(error);
    }
  }

  disable(){
    this.log.info(`disabling ${this.name} ${this.type}`);
    if (!this.enabled) return Promise.resolve()
    if (this.disabling) return new Promise((resolve, reject) => this.once('disabled', resolve))
    if (this.enabling) return new Promise((resolve, reject) => this.once('disabled', () => this.disable().then(resolve).catch(reject)))

    this.disabling = true;
    try {
      let promise = this._disable(mesh);
      if (promise instanceof Promise) return promise.then(() => {
        this.log.info(`${this.name} ${this.type} disabled`);
        this.disabling = false;
        this.enabled = false;
        this.emit('disabled', this);
      });
      else throw new Error(`${this.name}._disable() returned '${typeof promise}' instead of a promise.`);
    } catch (error) {
      this.log.error(error);
      this.emit('error', error);
      return Promise.reject(error);
    }
  }

  _enable(){
    let error = new Error(`${this.name}._enable() undefined`);
    this.log.error(error)
    this.log.info(`*** implement a function returning a Promise that enables this middleware ***`);
    process.nextTick(() => this.emit('error',error));
    return Promise.reject(error);
  }

  _disable(){
    let error = new Error(`${this.name}._disable() undefined`)
    this.log.error(error);
    this.log.info(`*** implement a function returning a Promise that disables this middleware ***`);
    process.nextTick(() => this.emit('error',error));
    return Promise.reject(error);
  }
}
