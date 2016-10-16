const TelehashAbstractMiddleware = require("./abstract.js");

class TelehashParserMiddleware extends TelehashAbstractMiddleWare {
  constructor(){
    super();

    this._enabled = false;
    this._parsed = 0;
  }

  get type() { return "parser"; }

  get _schema(){
    return this.JOI.object().keys({
      passthrough : this.JOI.boolean().default(false).description("successfully parsing a packet will not prevent other parsers from running")
    }).notes([
      "passthrough parsers are always run first and in parallel",
      "non passthrough parsers (default) will dynamically sorted based on the number of successfully parsed packets since application start"
    ])
  }

  get _enable(){
    this._enabled = true;
    this._parsed = 0;
    return Promise.resolve();
  }

  get _disable(){
    this._enabled = false;
    return Promise.resolve();
  }

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
      label : this.JOI.string().min(1).max(32).required(),
      id : this.JOI.shortHashname().required(),
      hashname : this.JOI.hashname().optional(),
      link : this.JOI.object().keys({
        id : this.JOI.shortHashname().required(),
        hashname : this.JOI.hashname().required()
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
    /*
    saving for raw parser

    .keys({
      json : this.JOI.object().keys({
        type : this.JOI.string().min(1).only("event").required().description("the telehash channel type"),
        c : this.JOI.number().label("Channel ID").required().description("the numerical identifier of this channel instance"),
        label : this.JOI.string().min(1).required().description("the user set label for the event"),
        from : this.JOI.shortHashname().required()
      }).required().description("the parsed JSON portion of the inbound packet"),
      body : this.JOI.binary().optional().description("the binary Buffer payload of the inbound packet")
    }).required().description("the raw packet payload");
    */
  }

  get process(link, {json, body}){
    this.parse(link, {json, body})
        .then((data) => ({
          label : json.label,
          id : json.from,
          //TODO: lookup hashname from somewhere
          link : {id : link.id, hashname : link.hashname},
          dataType : this.dataType,
          data
        }))
        .then((evt) => TelehashAbstractMiddleware.validateSchema(this.eventSchema, evt))
  }

  parse(link, packet){
    var error;
    try{
      let promise = this._parse(link, packet);
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

  _parse(link, packet){
    let warning = `${this.name}._parse(link, packet) is undefined`
    this.log.warn(warning);
    this.log.trace(warning);
    return Promise.reject(new Error(warning));
  }

}
