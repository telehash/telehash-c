var dgram = require('dgram');
var th = require("./index.js");

var client = dgram.createSocket({ type: 'udp4', reuseAddr: true });
client.on('error', function(err){
  console.error('udp4 socket fatal error',err);
});

process.stdin.on('data',function(data){
  var pkt = th.lob_new();
  th.lob_set(pkt,"is","stdin");
  th.lob_body(pkt,data,data.length);
  var raw = th.BUFFER(th.lob_raw(pkt),th.lob_len(pkt));
  client.send(raw,0,raw.length,42424,"127.0.0.1",function(err){
    console.log("sent",raw,err?err:"");
  });
});

