var th = require("../include/index.js");
th.util_sys_logging(1);

var mesh = mesh_new();
var id = mesh_generate(mesh);
console.log(lob_json(id));
var disco = hashname_im(mesh_keys(mesh),0x1a);
console.log(th.BUFFER(lob_raw(disco),lob_len(disco)));

// make signing self
var self = e3x_self_new(e3x_generate(),null);
var jwk = lob_new();
lob_set(jwk,"kty","EC");
lob_set(jwk,"crv","P-256");
jwk_local_get(self,jwk,true);
console.log("JWK:",JSON.stringify(lob_json(jwk)));

// create signed jwt
var jwt = lob_new();
lob_set(jwt,"alg","ES256");
lob_set(jwt,"typ","JWT");
var claims = lob_new();
lob_set_int(claims,"sub",42);
lob_link(jwt,claims);
jwt_sign(jwt,self);
console.log(jwt_encode(jwt));

// create remote
var rkey = '{"kty":"EC","crv":"P-256","x":"miVelWJPjn5WqK-h2rNRBZvJ6KyurmBKAtsN3DZCWTc","y":"vHoJm2aXc1c4Bz5BO7mJwx_kg4Lp-46GPRYSojXYy04","d":"KC68juQbaWgucbbwy1zYSyxwYWVYgd7EXEsI6vvA15o"}';
var rjwk = lob_new();
lob_head(rjwk,rkey,rkey.length);
var remote = jwk_remote_load(rjwk);

// encrypt to remote
var ckey = Buffer.alloc(32,0);
ckey.write("just testing");
var jwe = jwe_encrypt_1c(remote, jwt, ckey);
console.log("JWE:",lob_json(jwe));

// become remote and decrypt
var rself = jwk_local_load(rjwk);
ckey.fill(0);
jwt = jwe_decrypt_1c(rself,jwe,ckey);
console.log("CONTENT KEY:",ckey.toString());

// was decrypted jwt valid?
var ok = jwt_verify(jwt,jwk_remote_load(jwk));
console.log(ok?"SUCCESS":"FAIL");
