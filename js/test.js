var th = require("./index.js");

var mesh = mesh_new();
var id = mesh_generate(mesh);
console.log(lob_json(id));
var disco = hashname_im(mesh_keys(mesh),0x1a);
console.log(th.BUFFER(lob_raw(disco),lob_len(disco)));

