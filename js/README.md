Telehash-c + Emscripten = fast javascript telehash
===

to build, first get (emsdk)[https://kripken.github.io/emscripten-site/docs/getting_started/downloads.html]

`make javascript`

Basic
----
```
var th = require("telehash-core")

var lob = require("lob-enc");

var pkt = lob.encode({json:"stuff"}, new Buffer("payload"));

var l = th.lob_parse(pkt, pkt.length);

th.lob_json(l)                         // {json: "stuff"}
var pkt = th.BUFFER(th.lob_raw(l),th.lob_len(l)) // <Buffer 00 11 7b 22 73 6f 6d 65 22 22 7d 68 65 79 20 6c 6f 6f 6b 20 61 20 70 61 79 6c 6f 61 64>

var decoded = lob.decode(pkt)

```


Notes
====
- All functions in telehash.c are exported (once we know exactly which one's are necessary we can optimize the build )
- emscripten basically expects everything to be a number, pointers are numbers that reference the internal heap
- to ease use, all functions are dynamically wrapped such that you can use strings/buffers as arguments instead of manually using uint8* or char*
- if you find yourself needing the low level version of any function it is available as th._function_name
- return values default to their numerical type unless specified at the top of index.js, if you add a key/value pair with the underscore version of the function name as the key and the value as "string" or "json", the conversion out of emscripten land will be done for you (buffers need to be done manually because you need to manually get the length)
- for a raw buffer/byte array, use th.BUFFER(ptr, len) to get a nodejs buffer easily
- th.CALLBACK(function (number, string, json), ["number","string","json"]) will wrap a function to give you the same sort of type conversion as with return values... if you don't need it and can just write the function to take the raw values, all the functions that accept callbacks will automatically turn them into function pointers internally, no need to use this wrapper.

Roadmap
====
- add something similar to the returntypes object pattern to declare which functions expect the caller to free the arguments. specifically since strings and buffers need to be imported into emscripten land, if they are not needed after the function exits, they should be freed. normally this would happen on the C stack, but since we're mallocing to bridge between the javascript we need to come up with a strategy to do this somewhat elegantly.
