This may not even be feasible, but it's an attempt to get telehash working on the pebble watch. It's arm-based and has a limited libc (https://sourceware.org/newlib/libc.html, nm libc.a | grep " T " | awk '{ print $3 }').

Here's the current (non-functional) build by copying `lib/*` and `pebble/*` into the `~/pebble-dev/PebbleSDK-current/Examples/watchapps/feature_stdlib/src` dir and running `pebble build`:

Memory usage:
=============
Total app footprint in RAM:      18424 bytes / ~24kb
Free RAM available (heap):        6152 bytes



