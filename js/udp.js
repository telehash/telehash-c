var dgram = require('dgram');
var os = require('os');
var th = require("./index.js");

var mesh = mesh_new();
mesh_generate(mesh);
console.log(hashname_short(mesh_id(mesh)));

mesh_on_link(mesh, "meeseeks", function(link){
  console.log(hashname_short(link_id(link)),link_up(link)?"up":"down");
});

mesh_on_open(mesh, "meeseeks", function(link, open){
  console.log("open",lob_get(open,"type"));
  if(lob_get(open,"type") == "stdin")
  {
    var body = th.BUFFER(lob_body_get(open),lob_body_len(open));
    console.log(body.toString());
  }
});

var server = dgram.createSocket({ type: 'udp4', reuseAddr: true }, function receive(msg, from)
{
  var packet = lob_parse(msg,msg.length);
  if(lob_head_len(packet) > 6)
  {
    if(hashname_vkey(packet,0x1a))
    {
      link = link_get_key(mesh,packet,0x1a);
      console.log("establishing link with",hashname_short(link_id(link)));

      // send discovery hello back
      var hello = hashname_im(mesh_keys(mesh),0x1a);
      var raw = th.BUFFER(lob_raw(hello),lob_len(hello));
      server.send(raw,0,raw.length,from.port,from.address,function(err){
        if(err) console.error("error sending to",from,err);
      });
      
      // plumb a pipe
      link_pipe(link, function(link, packet, arg){
        if(!packet) return null;
        var raw = th.BUFFER(lob_raw(packet),lob_len(packet));
//        console.log("send",link,packet,from,raw,raw.length);
        server.send(raw,0,raw.length,from.port,from.address,function(err){
          if(err) console.error("error sending to",from,err);
        });
      },null);
    }else{
      console.error("unknown raw packet",lob_json(packet));
    }
  }else{
    mesh_receive(mesh,lob_parse(msg,msg.length));
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



