const EnvirEmitter = require('enviromitter');
const Mesh = require("../core.js").Mesh;
const Link = require("../core.js").Link;
const JOI = EnvirEmitter.JOI;

const authorSchema = JOI.object().keys({
  name : JOI.string().required().description("author name"),
  email : JOI.string().email().description("author email address"),
  github_user : JOI.string(),
  git : JOI.string().uri({ scheme: [ 'git', /git\+https?/ ]})
}).required().label("Author").description("authorship info for this middleware")

class TelehashAbstractMiddleware extends EnvirEmitter {
  static get Mesh(){return Mesh;}
  static get Link(){return Link;}

  static get optionsSchema(){
    return EnvitEmitter.optionsSchema.concat(EnvirEmitter.JOI.object())
  }

  static Test(CHILD, OPTIONS){
    class TelehashMiddleware extends TelehashAbstractMiddleware {
      constructor(){
        super();
      }

      get _type() { return "TypeTest"; }

      get _name() { return "AbstractTest"}

      get _environment(){ return {runtime : ["any"], platform : ["any"], internet : true, gps : false}}

      get _author() { return ({
        name : "Ryan Bennett",
        github_user : "rynomad",
        git : "git@github.com:telehash/telehash-c",
        email : "nomad.ry@gmail.com"
      })}

      _enable(){
        return Promise.resolve()
      }

      _disable(){
        return Promise.resolve()
      }
    }

    return EnvirEmitter.Test(CHILD || TelehashMiddleware, OPTIONS)
                       .then((instance) => new Promise((resolve, reject) => {
                         instance.log.info('completed EnvirEmitter test');
                         let mesh = new Mesh(() => {})
                         instance.enable(mesh)
                                 .then(() => instance.reset())
                                 .then(() => instance.disable({cause : "test", test : "passing"}))
                                 .then(() => resolve(instance)).catch(reject);

                       }))

  }

  constructor(options){
    super(options);
  }


  get _optionsSchema(){
    return TelehashAbstractMiddleware.optionsSchema;
  }

  get type(){
    let type = this._type;
    if (typeof type === "string") return type;

    let error = new Error('._type must be a string, got ${typeof type}');
    this.log.info(`*** fix the ._type getter so that it returns a string ***`);
    this.log.error(error);
    process.nextTick(() => this.emit('error', error));
    return type;
  }

  get author(){
    let valid = authorSchema.validate(this._author);
    if (!valid.error) return valid.value;


    this.log.error(valid.error.annotate());
    this.log.error(valid.error);
    this.emit('error', valid.error);
    return this._author;
  }

  get _author(){
    this.log.warn(`._author undefined`);
    this.log.info([
      `*** implement an ._author getter that returns an object with the following keys:`
    ].concat(TelehashAbstractMiddleware.annotateSchema(authorSchema)._notes).join('\n\t'))
    return {name : "anonymous"};
  }

  get _type (){
    var error = new Error(`._type undefined`);
    this.log.error(error);
    this.log.info(`*** implement a ._type getter that returns a string of the middleware type *** `)
    process.nextTick(() => this.emit('error', error));
    return "abstract";
  }

  _validateEnableArgs(arg){
    if (arg instanceof Mesh) return Promise.resolve(arg);

    return Promise.reject(new Error(`._validateEnableArg(arg) expected instanceof Mesh, got ${typeof arg}`))
  }
}

try{
  if (require.main === module) {
    console.log('called directly');
    TelehashAbstractMiddleware.Test().then((env) => {
      console.log('passed self test');
      process.exit(0);
    })
    .catch(e => {
      console.log("failed self test", e)
      process.exit(1);
    })
  }
}catch(e){}

module.exports = TelehashAbstractMiddleware;
