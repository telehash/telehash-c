var dgram = require('dgram');
var os = require('os');
var th = require("./index.js");

var mesh = mesh_new();
mesh_generate(mesh);
console.log(hashname_short(mesh_id(mesh)));

var server = dgram.createSocket({ type: 'udp4', reuseAddr: true }, function receive(msg, rinfo)
{
  var packet = lob_parse(msg,msg.length);
  if(lob_head_len(packet) > 6)
  {
    if(hashname_vkey(packet,0x1a))
    {
      link = link_get_key(mesh,packet,0x1a);
      console.log("establishing link with",hashname_short(link_id(link)));
    }else{
      console.error("unknown raw packet",lob_json(packet));
    }
  }
});

server.bind(42424, function(err){
  if(err) return console.error('bind failed',err);
  var address = server.address();
  var ifaces = os.networkInterfaces()
  for (var dev in ifaces) {
    ifaces[dev].forEach(function(details){
      if(details.family != 'IPv4') return;
      console.log({ip:details.address,port:address.port});
    });
  }
});

server.on('error', function(err){
  console.error('udp4 socket fatal error',err);
});



