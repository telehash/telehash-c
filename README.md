telehash tools library in c
===========================

This is a full implementation of telehash in portable c, designed to be a library for apps to link with, source that can be copied into other projects, and usable components for embedded systems.

## Building

Just run `make` to build a `libtelehash.a` and some utility apps into `bin/*`.  Use `make test` to run a full test suite.

Use `npm install` to automatically install optional crypto dependencies (libsodium and libtomcrypt).

## Library Interface

The codebase is a set of components that can be used to create/integrate telehash services, here's a rough layout, see [src/](src/) and [include/](include/) for details.

* `e3x_*`: all of the crypto handling
* `mesh_*` and `link_*`: higher level easy interfaces for apps
* `ext_*`: various useful extensions to a mesh to support built-in channels
* `pipe_*` and `net_*`: transport and networking handling
* `util_*` and [libs](include/lib.h): portable utilities and bundled libs

There's many examples of usage in the bundled [tests](test/).