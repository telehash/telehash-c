var dgram = require('dgram');
var th = require("./index.js");

var link = 0;
var client = dgram.createSocket({ type: 'udp4', reuseAddr: true }, function receive(msg, rinfo)
{
  var ret = mesh_receive(mesh,lob_parse(msg,msg.length));
  if(ret)
  {
    if(!link) console.log("new link to",hashname_short(link_id(link)));
    link = ret;
  }
});

client.on('error', function(err){
  console.error('udp4 socket fatal error',err);
});

var mesh = mesh_new();
mesh_generate(mesh);
console.log(hashname_short(mesh_id(mesh)));

// send our key as a discovery hello
var hello = hashname_im(mesh_keys(mesh),0x1a);
var raw = th.BUFFER(lob_raw(hello),lob_len(hello));
client.send(raw,0,raw.length,42424,"127.0.0.1",function(err){
  console.log("sent hello",err?err:"");
});

process.stdin.on('data',function(data){
  if(!link) return console.error("no link");
  var pkt = lob_new();
  lob_set(pkt,"type","stdin");
  lob_body(pkt,data,data.length);
  
  var raw = th.BUFFER(lob_raw(pkt),lob_len(pkt));
  client.send(raw,0,raw.length,42424,"127.0.0.1",function(err){
    console.log("sent",raw,err?err:"");
  });
});

