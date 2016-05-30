var th = require("./index.js");

var mesh = th.mesh_new(3);
var id = th.mesh_generate(mesh);
console.log(th.lob_json(id));