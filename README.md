telehash tools library in c
===========================

This is a full implementation of telehash in portable c for embedded systems, designed to be source that can be copied into other projects.

## Building

Just run `make` to build a `libtelehash.a` and some utility apps into `bin/*`.  Use `make test` to run a full test suite, and `make static` to generate a current standalone `telehash.c` and `telehash.h`.

Use `npm install` to automatically install optional crypto dependencies (libsodium and libtomcrypt).

## Library Interface

The codebase is a set of components that can be used to create/integrate telehash services, here's a rough layout, see [src/](src/) and [include/](include/) for details.

* `e3x_*`: all of the crypto handling
* `mesh_*` and `link_*`: higher level easy interfaces for apps
* `ext_*`: various useful extensions to a mesh to support built-in channels
* `pipe_*` and `net_*`: transport and networking handling
* `util_*` and [libs](include/lib.h): portable utilities and bundled libs

There's many examples of usage in the bundled [tests](test/).

### Memory Notes

Most of the codebase uses [lob_t](include/lob.h) as the primary data type since it handles JSON and binary for all packets.

The following methods return generated lobs (you must free):

* e3x_channel_receiving
* e3x_channel_sending
* e3x_channel_oob
* e3x_channel_packet
* e3x_self_decrypt
* e3x_exchange_message
* e3x_exchange_handshake
* e3x_exchange_receive
* link_handshakes
* link_sync
* link_resync
* mesh_on_open (callback must return or free the given lob)

And these methods consume lobs (will be free'd):

* e3x_channel_receive
* e3x_channel_send
* link_handshake
* link_receive
* link_receive_handshake
* link_send
* link_flush
* link_direct
* mesh_handshake
* mesh_handshakes
* mesh_receive
* mesh_receive_handshake
