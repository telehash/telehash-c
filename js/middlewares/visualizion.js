const TelehashIntegrationMiddleware = require('./integration');
const JOI = require('joi');
const environmentSchema = JOI.object().keys({
  internet : JOI.any().valid('required',true,false).description("does this visualization use an internet connection?"),
  platform : JOI.string().
})


class TelehashVisualizationMiddleware extends TelehashIntegrationMiddleware{
  constructor(){
    super()
  }

  get _type() { return "visualizer"; }

  get environment(){
    let env = this._environment;
    if (typeof env === "string") return env;

    let error = new Error(`${}`)
  }

  get _environment(){
    let error = new Error(`${this.name} visualizer has not provided `)
    this.log.err
  }

  getReady(){
    let promise = this._getReady();
  }

  _getReady(){
    let error = new Error(``)
    return Promise.reject()
  }
}
