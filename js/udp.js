var dgram = require('dgram');
var os = require('os');
var th = require("./index.js");

var server = dgram.createSocket({ type: 'udp4', reuseAddr: true }, function(msg, rinfo){
  var packet = th.lob_parse(msg,msg.length);
  var body = th.BUFFER(th.lob_body_get(packet),th.lob_body_len(packet));
  console.log(th.lob_json(packet),body.toString());
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



