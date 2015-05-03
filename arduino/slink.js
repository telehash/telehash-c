var device = process.argv[2] || '/dev/ttyUSB0';
var SerialPortStream = require('serial-port-stream');
var stream = new SerialPortStream(device, { baudRate : 115200 });
stream.pipe(process.stdout);
process.stdin.pipe(stream);