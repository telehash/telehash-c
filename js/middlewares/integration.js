const TelehashAbstractMiddleware = require("./abstract.js");

class TelehashIntegrationMiddleware extends TelehashAbstractMiddleware {
  constructor(){
    super();
  }

  get _type() { return "integration"; }

  get _schema() {
    return this.integrationSchema();
  }

  get integrationSchema(){
    let schema = this._integrationSchema;
    if (schema && schema.isJoi) return this.baseSchema.concat(schema);

    let error = new Error(`${this.name}._integrationSchema is not a JOI object`);
    this.log.error(error);
    this.emit('error', error);
    return this.JOI.object();
  }

  get _integrationSchema(){
    this.log.warn(`${this.name}._integrationSchema undefined`);
    this.log.trace(`${this.name}._integrationSchema undefined`);
    return this.JOI.object();
  }

  get baseSchema(){
    return this.JOI.object().keys({
      dataTypes : this.JOI
                      .array()
                      .items(this.JOI.string().valid(this.mesh.parsers.map(({dataType}) => dataType))
                      .required()
                      .description(`The data types that will be handled by this ${this.name} integration`)
                      .label("Data Types"),
      events : this.JOI.array()
                       .items(this.JOI.string())
                       .default(["*"],"activate on all event labels")
                       .description(`The event labels that will be handled by this ${this.name} integration`)
                       .label("Events"),
      id : this.JOI.string()
                   .required()
                   .description(`a unique id for this instance of ${this.name} integration`)
                   .label("ID")
    }).required()
  }

  handleEvent(event){
    var error;
    try{
      let promise = this._handleEvent(event);
      if (promise instanceof Promise) return promise;
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
