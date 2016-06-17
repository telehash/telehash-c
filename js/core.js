'use strict';
const EventEmitter = require("events").EventEmitter;
const fs = require("fs");
const lob = require("lob-enc");
//const hashname = require("hashname");
//const base32 = require("base32")
const path = require("path");
const Duplex = require("stream").Duplex;

const th = require("./include/");
//th.util_sys_logging(1);
const lob_to_c = (buf) => typeof buf === "number" ? buf : lob_parse(buf, buf.length)

const lob_from_c = (ptr) => lob.decode(buf_lob_from_c(ptr))

const buf_lob_from_c = (ptr) => th.BUFFER(lob_raw(ptr),lob_len(ptr))

const hex_to_lob = (hex) => {
  let k = new Buffer(hex, "hex");
  return lob_parse(k, k.length);
}



class Chunks{
  constructor(mesh, stream, chunk_size){
    var link;
    var linked = false;
    var that = this;
    this.chunks = lob.chunking({size:chunk_size, blocking:true}, function receive(err, packet){
      if(err || !packet)
      {
        console.error('pipe chunk read error',err,pipe.id);
        return;
      }
      link = mesh_receive(mesh,lob_to_c(packet))
      if (!linked && link){
        linked = true;
        link_pipe(link, function(link, packet, arg){
          if(!packet) return null;
          that.chunks.send(lob_from_c(packet));
          return link;
        },null);
      }
      
    });

    this.chunks.pipe(stream);
    stream.pipe(this.chunks);
    var greeting = buf_lob_from_c(mesh_json(mesh));
    this.chunks.send(greeting);
  }
}

class Frames{
  constructor (mesh, stream, frame_size){
    this.frames = util_frames_new(frame_size);
    var frames = this.frames;
    var interval;
    function frames_flush()
    {
      if(util_frames_pending(frames))
      {      
        try {
          let frame = th._malloc(frame_size);
          util_frames_outbox(frames,frame,null);
          stream.write(th.BUFFER(frame,frame_size));
          util_frames_sent(frames);
          th._free(frame);          
        } catch (e){
          clearInterval(interval);
        }

      }

    }

    var linked;
    var readbuf = new Buffer(0);

    stream.on('data',function(data){
      // buffer up and look for full frames
      readbuf = Buffer.concat([readbuf,data]);
      //console.log(readbuf,data);
      while(readbuf.length >= frame_size)
      {
        // TODO, frames may get offset, need to realign against whatever's in the buffer
        //th.util_sys_logging(1);
        var frame = readbuf.slice(0,frame_size);
        readbuf = readbuf.slice(frame_size);
        util_frames_inbox(frames,frame,null);
        //console.log("received a frame",frame);
      }
      // check for and process any full packets
      var packet;
      while((packet = util_frames_receive(frames)))
      {
        //console.log("got packet")
        //th.util_sys_logging(1);
        var link = mesh_receive(mesh,packet);
        if(linked || !link) continue;
        mesh_link(mesh, link);

        // catch new link and plumb delivery pipe
        linked = link;
        link_pipe(link, function(link, packet, arg){
          if(!packet) return null;
          //console.log("sending packet", frames, packet, lob_len(packet));
          util_frames_send(frames,packet)
          return link;
        },null);
        
      }
    });

    util_frames_send(frames,mesh_json(mesh));
    interval = setInterval(frames_flush,5)

    
  }
}

class Channel extends Duplex{
  constructor(c_mesh, c_link, open){
    super({objectMode : true});
    let buf = lob.encode(open.json, new Buffer(open.body || 0));
    let c_lob = lob_to_c(buf);

    this._c_mesh = c_mesh;

    this._c_chan = link_chan(c_link, c_lob);

    this._open = c_lob;

    chan_handle(this._c_chan, () => {
      let c_lob;
      while(c_lob = chan_receiving(this._c_chan)) {
        let lob = lob_from_c(c_lob);
        //console.log("LOB", lob)
        if (lob)
          this.push(lob);
        else
          console.log("bad lob",buf_lob_from_c(c_lob).toString())

      }
    }, null);

    this.on('data',(packet) => {
      this.receiving(null, packet, () => {});
    })
  }

  receiving(er, packet, cbChan){
    //console.log("RECIEVING??????")
  }

  send(packet){
    this.write(packet);
  }

  c_send(c_lob){
    chan_send( this._c_chan, c_lob );
  }

  _write(packet, enc, cb){
    packet.json.c = chan_id(this._c_chan);
    this.c_send(lob_to_c(lob.encode(packet.json, packet.body))); 
    cb();
  }

  _read(size){
    //console.log("_READ")
  }
}

class Link extends EventEmitter {
  constructor(Mesh, c_link){
    super();
    this._c_mesh = Mesh._mesh;
    this._c_link = c_link;
    this.hashname = th.UTF8ToString( hashname_char(link_id(c_link)) );
    Mesh._links.set(this.hashname.substr(0,8), this);
    this.on('down',() => {
      Mesh._links.delete(this.hashname.substr(0,8))
      link_pipe(c_link,null,null);
    })
    this.x = {
      channel : this.channel
    }
  }

  direct(open, body){
    let buf = lob.encode(open, new Buffer(body));
    link_direct( this._c_link, lob_to_c(buf) );
  }

  init(file, cb){
    let chan = this.channel({json : {type: "init"} , body : file})
    chan.on('data',(packet) => cb(null, packet.json))
    chan.c_send(chan._open);
  }

  channel(open){
    return new Channel( this._c_mesh, this._c_link, open);
  }

  console(cmd, cb){
    let chan = this.channel({json : {type : "console"}, body : cmd})
    chan.on('data',(packet) => cb(null, Object.assign(packet.json, {body: packet.body ? packet.body.toString() : null})))
    chan.c_send(chan._open);
  }
}

class Mesh extends EventEmitter {
  constructor(on_up, opts){
    super();
    this._mesh = mesh_new();
    this._open = true;
    this._opts = opts;
    
    this._accept = new Set();
    this._reject = new Set();
    this._links = new Map();
    this._frames = new Set();
    this._chunks = new Set();
    this._listen = new Set();
    this._connect = new Map();

    this.on("up",on_up);
  }

  frames(transport, frame_size){
    this._frames.add(new Frames(this._mesh, transport, frame_size));
  }

  chunks(transport, chunk_size){
    this._chunks.add(new Chunks(this._mesh, transport, chunk_size));
  }

  init(on_link, opts){

  }

  link(json){

  }

  accept(hashname){
    this._open = false;

  }

  reject(hashname){

  }

  listen(){
    let promise = [];
    for (let listen of this._listen) promise.push(listen());
    Promise.all(promise).then(() => this.emit('up', this)).catch((e) => this.emit('error', e))
  }

  start(){
    this._mesh = mesh_new();

    mesh_on_discover(this._mesh, "*", (mesh, discovered, pipe) => {
      let hashname = lob_from_c(discovered).json.hashname;
      if (!this._reject.has(hashname) && this._open || this._accept.has(hashname)) mesh_add(mesh, discovered, pipe);
    });

    mesh_on_link(this._mesh, "*", (c_link) => {
      if (!this._links.has(th.UTF8ToString( hashname_char(link_id(c_link)) ).substr(0,8)))
        this.emit("link", new Link(this , c_link));
    });

    mesh_on_open(this._mesh, "*", (c_link, c_open) => 
      this.emit('open', this._links.get( th.UTF8ToString( hashname_char(link_id(c_link)) ).substr(0,8) ) , lob_from_c(c_open) ) 
    );

    this._getKeys((secrets, keys) => {
      if (secrets && keys){
        mesh_load(this._mesh,  hex_to_lob(secrets),hex_to_lob(keys))
        this.listen();
      } else {
        let secrets = mesh_generate(this._mesh)
        if (!secrets) throw new Error("mesh generate failed");

        this._storeKeys( buf_lob_from_c(secrets).toString("hex"), buf_lob_from_c(lob_linked(secrets)).toString("hex"), () => {
          this.listen();
        })
      }
    })
  }

  _getKeys(cb){
    let { TELEHASH_SECRET_KEYS, TELEHASH_PUBLIC_KEYS} = process.env;
    cb(  TELEHASH_SECRET_KEYS, TELEHASH_PUBLIC_KEYS)
  }

  _storeKeys(secrets, keys, cb){
    cb()
    // this function intentionally left blank
  }

  use(middleware){
    let mid = middleware(this, th);
    switch (mid.type){
      case "transport":
        if (mid.listen) this._listen.add(mid.listen);
        if (mid.connect) this._connect.set(mid.type, mid.connect);
        return;
      case "channel":
        return this.on('open', mid.open.bind(this) );
      case "keystore":
        this._getKeys = mid.getKeys;
        this._storeKeys = mid.storeKeys;
      default:
        return;
    }
  }
}
var _crypto;
const random = () => new Promise((res, rej) => {
  let crypto;
  if (crypto && crypto.getRandomBytes){
    return crypto.getRandomBytes(new Uint8Array)[0]
  } else {
    if (!_crypto) _crypto = require("crypto")

    var _buf = new ArrayBuffer(4)
    var u32 = new Uint32Array(_buf);
    var u8 = new Uint8Array(_buf);
    var bytes = crypto.randomBytes(4)
    for (var i = 0; i < 4; i++) u8[i] = bytes[i];
    return u32[0]
  }
})


const defaultOpts = {
  id : "./mesh.hashname",
  chan :[],
  transport : []
}

const Telehash = module.exports = (on_up, opts) => {
  if (!on_up){
    throw new Error("must provide an on_link handler");
  }

  opts = Object.assign({}, defaultOpts, opts || {});

  return new Mesh(on_up, opts)
} 
