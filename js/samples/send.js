var dgram = require('dgram');
var th = require("./index.js");

var link = 0;
var client = dgram.createSocket({ type: 'udp4', reuseAddr: true }, function receive(msg, from)
{
  var packet = lob_parse(msg,msg.length);
  if(hashname_vkey(packet,0x1a))
  {
    link = link_get_key(mesh,packet,0x1a);
    console.log("establishing link with",hashname_short(link_id(link)));
    link_pipe(link, function(link, packet, arg){
      if(!packet) return null;
      var raw = th.BUFFER(lob_raw(packet),lob_len(packet));
//      console.log("send",link,packet,from,raw);
      client.send(raw,0,raw.length,from.port,from.address,function(err){
        if(err) console.error("error sending to",from,err);
      });
    },null);
  }else{
    mesh_receive(mesh,lob_parse(msg,msg.length));
  }
});

client.on('error', function(err){
  console.error('udp4 socket fatal error',err);
});

var mesh = mesh_new();
mesh_generate(mesh);
console.log(hashname_short(mesh_id(mesh)));
mesh_on_link(mesh, "foo", function(link){
  console.log(hashname_short(link_id(link)),link_up(link)?"up":"down");
});

// send our key as a discovery hello
var hello = hashname_im(mesh_keys(mesh),0x1a);
var raw = th.BUFFER(lob_raw(hello),lob_len(hello));
client.send(raw,0,raw.length,42424,"127.0.0.1",function(err){
  console.log("sent hello",err?err:"");
});

process.stdin.on('data',function(data){
  if(!link) return console.error("no link");
  var packet = lob_new();
  lob_set(packet,"type","stdin");
  lob_body(packet,data,data.length);
  link_direct(link,packet);
});

