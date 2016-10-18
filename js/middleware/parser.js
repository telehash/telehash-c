const TelehashAbstractMiddleware = require("./abstract.js");
const JOI = TelehashAbstractMiddleware.JOI;

class TelehashParserMiddleware extends TelehashAbstractMiddleware {
  static Test(CHILD, OPTIONS){
    class ParserTest extends TelehashParserMiddleware{
      constructor(){
        super();
      }
      get _dataType(){ return "mqttsn|cbor|json|raw|opc|modbus|...|";}

      get _name(){ return "Test" }

      get _environment(){ return {internet : false, gps : false}}

      _enable(){
        this._enabled = true;
        return Promise.resolve();
      }

      _disable(){
        this._enabled = false;
        return Promise.resolve();
      }

      get _dataSchema(){
        return JOI.object().keys({
          json : this.JOI.object().keys({
            type : this.JOI.string().min(1).only("event").required().description("the telehash channel type"),
            c : this.JOI.number().label("Channel ID").required().description("the numerical identifier of this channel instance"),
            label : this.JOI.string().min(1).required().description("the user set label for the event"),
            from : this.JOI.string().min(8).max(8).regex(/[A-Za-z2-7]*/).required()
          }).required().description("the parsed JSON portion of the inbound packet"),
          body : this.JOI.binary().optional().description("the binary Buffer payload of the inbound packet")
        }).required().description("the raw packet payload");
      }

      _parse(link, packet){
        this.log.debug('test parser got packet',packet);
        return Promise.resolve(packet)
      }

    }

    return TelehashAbstractMiddleware.Test(CHILD || ParserTest, OPTIONS )
                             .then((instance) => {
                               console.log("telehashtest passed", instance);
                               return instance.process({
                                 id : "63xpe4is",
                                 hashname : "63xpe4isnc5b7sa6n37w5j4x3q7q2qjmi4ufnminklrn6a6vvvta"
                               },{
                                 json : {
                                   type : "event",
                                   c : "5",
                                   label : "giga",
                                   from : "7654tyru"
                                 }, body : (new Buffer(40))
                               }).then((res) => {
                                 console.log("got raw", res);
                                 return instance;
                               })
                             })
  }

  static get optionsSchema(){
    return TelehashAbstractMiddleware.optionsSchema.concat(JOI.object().keys({
      passthrough : this.JOI.boolean().default(false).description("successfully parsing a packet will not prevent other parsers from running")
    }).notes([
      "passthrough parsers are always run first and in parallel",
      "non passthrough parsers (default) will dynamically sorted based on the number of successfully parsed packets since application start"
    ]))
  }

  static get _optionsSchema(){
    return TelehashParserMiddleware.optionsSchema;
  }

  constructor(){
    super();
  }

  get _type() { return "parser"; }

  get dataType() {
    let type = this._dataType;
    if (typeof type === "string") return type;

    let error = new Error('._type must be a string');
    process.nextTick(() => this.emit('error', error));
    return type;
  }

  get _dataType(){
    var error = new Error(`${this.name}._dataType undefined`);
    process.nextTick(() => this.emit('error', error));
    return "undefined";
  }

  get eventSchema(){
    return  JOI.object().keys({
      label : this.JOI.string().min(1).max(32),
      id : this.JOI.string().min(8).max(8).regex(/[A-Za-z2-7]*/).required(),
      hashname : this.JOI.string().min(52).max(52).regex(/[A-Za-z2-7]*/).optional(),
      link : this.JOI.object().keys({
        id : this.JOI.string().min(8).max(8).regex(/[A-Za-z2-7]*/).required(),
        hashname : this.JOI.string().min(52).max(52).regex(/[A-Za-z2-7]*/).required()
      }).required().description("link id and full hashname of the link that delivered the data"),
      dataType : this.JOI.string().only(this.dataType).required(),
      data : this.dataSchema
    })
  }

  get dataSchema(){
    let schema = this._dataSchema;
    if (schema && schema.isJoi) return schema;

    let error = new Error(`${this.name}._dataSchema must be a JOI schema, got ${typeof schema}`);
    process.nextTick(() => this.emit('error', error));
    return this.JOI.object().required().description("the parsed data payload");
  }

  get _dataSchema(){
    let error = new Error(`${this.name}._dataSchema is undefined`);
    this.log.error(error);
    process.nextTick(() => this.emit('error',error));

    return this.JOI.object().description("no-op");
  }

  process(link, packet){
    this.log.debug(`.process(Link<${link.id}>, ${JSON.stringify(packet.json)}::${packet.body})`)
    return this.parse(link, packet)
                .then((data) => ({
                  label : packet.json.label,
                  id : packet.json.from,
                  //TODO: lookup hashname from somewhere
                  link : {id : link.id, hashname : link.hashname},
                  dataType : this.dataType,
                  data
                }))
                .then((evt) => {
                  this.log.debug(`.process() completed, validating result`,evt);
                  return TelehashParserMiddleware.validateSchema(this.eventSchema, evt)
                })
  }

  parse(link, packet){
    this.log.debug(`.parse(Link<${link.id}>, ${JSON.stringify(packet.json)}::${packet.body})`)
    var error;
    try{
      let promise = this._parse(link, packet);
      if (promise instanceof Promise) return promise;
    } catch (error) {
      error = e;
    }

    error = error || new Error(`${this.name}._parse(link, packet) returned '${typeof promise}' instead of a promise.`);
    this.log.error(error);
    this.emit('error', error);
    return Promise.reject(error);
  }

  _parse(link, packet){
    let warning = `${this.name}._parse(link, packet) is undefined`
    this.log.warn(warning);
    this.log.trace(warning);
    return Promise.reject(new Error(warning));
  }

}

try{
  if (require.main === module) {
    console.log('called directly');
    TelehashParserMiddleware.Test().then((env) => {
      console.log('passed self test');
      process.exit();
    })
    .catch(e => {
      console.log(e);
    })
  }
}catch(e){}

module.exports = TelehashParserMiddleware;
