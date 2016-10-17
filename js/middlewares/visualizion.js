const TelehashIntegrationMiddleware = require('./integration');
const JOI = require('joi');

class TelehashVisualizationMiddleware extends TelehashIntegrationMiddleware{
  constructor(){
    super()
  }

  get _type() { return "visualizer"; }

}
