const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI;

class TelehashIntegrationMiddleware extends TelehashAbstractMiddleware {


  static Test(CHILD, OPTIONS){
    class IntegrationTest extends TelehashIntegrationMiddleware{
      constructor(){
        super();
      }

      _handleEvent(event){
        return Promise.resolve(this);
      }
    }

    return TelehashMiddleware.Test(CHILD || IntegrationTest, OPTIONS || {id :"testid"})
                             .then((instance) => {
                               console.log("telehashtest passed");
                               return instance.handleEvent({
                                 type : "GPS",
                                 specific : "GPS"
                               })
                             })
  }

  static get optionsSchema(){
    return TelehashAbstractMiddleware.optionsSchema.concat(JOI.object().keys({
      dataTypes : JOI.array()
                      .items(JOI.string())
                      .default(['any'])
                      .description(`The data types that will be handled by this integration`)
                    .label("Data Types"),
      events : JOI.array()
                   .items(JOI.string().valid(["error","system","network","data"]))
                   .default(["data"])
                   .description(`The event labels that will be handled by this integration`)
                   .label("Events"),
      id : JOI.string()
               .required()
               .description(`a unique id for this integration`)
               .label("ID")
    }).required())
  }

  constructor(){
    super();
  }

  get _type() { return "integration"; }

  static get _optionsSchema(){
    return TelehashIntegrationMiddleware.optionsSchema;
  }

  handleEvent(event){
    var error;
    try{
      let promise = this._handleEvent(event);
      if (promise instanceof Promise) return promise.catch(e => {
        this.emit('error',e);
      });
    } catch (error) {
      error = e;
    } finally {
      error = error || new Error(`${this.name}._handleEvent(event) returned '${typeof promise}' instead of a promise.`);
      this.log.error(error);
      this.emit('error', error);
      return Promise.reject(error);
    }
  }

  _handleEvent(){
    let error = new Error(`${this.name}._handleEvent() undefined`);
    this.log.error(error);
    this.log.info(`*** impliment a function ._handleEvent that accepts  ***`)
    this.log.trace(warning);
    return Promise.reject(new Error(warning));
  }
}

try{
  if (require.main === module) {
    console.log('called directly');
    TelehashAbstractMiddleware.Test().then((env) => {
      console.log('passed integration self test');
      process.exit();
    })
    .catch(e => {
      console.log(e);
    })
  }
}catch(e){}

module.exports = TelehashIntegrationMiddleware;
