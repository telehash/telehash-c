var SerialPort = require('serialport');
var th = require("./index.js");
th.util_sys_logging(1);

var mesh = mesh_new();
mesh_generate(mesh);
console.log(hashname_short(mesh_id(mesh)));

mesh_on_discover(mesh, "capncrunch", mesh_add);

mesh_on_link(mesh, "capncrunch", function(link){
  console.log(hashname_short(link_id(link)),link_up(link)?"up":"down");
  // send test cmd
  var open = lob_new();
  lob_set(open,"type","console");
  var cmd = new Buffer("tap('led.blue')");
  lob_body(open,cmd,cmd.length);
  link_direct(link,open);
});

var frames = util_frames_new(64);
function frames_flush()
{
  var frame = th._malloc(64);
  while(util_frames_outbox(frames,frame,null))
  {
    port.write(th.BUFFER(frame,64));
    console.log("sent a frame",th.BUFFER(frame,64));
    if(!util_frames_sent(frames)) break; // no more to send, done
  }
  th._free(frame);
}

// open port and send hello
var port = new SerialPort(process.argv[2],{baudrate: 115200},function(err){
  if(err) return console.error("open failed to",process.argv[2],err);
  util_frames_send(frames,hashname_im(mesh_keys(mesh),0x1a));
  console.log("sent greeting");
  frames_flush();
});

// process any incoming data
var linked;
var readbuf = new Buffer(0);
console.log("READBUF",readbuf);
port.on('data',function(data){
  // buffer up and look for full frames
  readbuf = Buffer.concat([readbuf,data]);
  console.log(readbuf,data);
  while(readbuf.length >= 64)
  {
    // TODO, frames may get offset, need to realign against whatever's in the buffer
    var frame = readbuf.slice(0,64);
    readbuf = readbuf.slice(64);
    util_frames_inbox(frames,frame,null);
    console.log("received a frame",frame);
  }
  // check for and process any full packets
  var packet;
  while((packet = util_frames_receive(frames)))
  {
    var link = mesh_receive(mesh,packet);
    if(linked || !link) continue;

    // catch new link and plumb delivery pipe
    linked = link;
    link_pipe(link, function(link, packet, arg){
      if(!packet) return null;
      console.log("sending packet",lob_len(packet));
      util_frames_send(frames,packet);
      frames_flush();
      return link;
    },null);
    
  }
  frames_flush();
});


