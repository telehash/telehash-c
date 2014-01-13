telehash tools library in c
===========================

This is a growing set of tools for working with telehash in c.

It depends on ../[js0n](https://github.com/quartzjer/js0n)/* and [tomscrypt](http://github.com/libtom/libtomcrypt).

Testing tips:

```
git clone https://github.com/quartzjer/telehash-c.git
git clone https://github.com/quartzjer/js0n.git
git clone https://github.com/libtom/libtommath.git
git clone https://github.com/libtom/libtomcrypt.git
cd libtommath && sudo make install
cd libtomcrypt && CFLAGS="-DLTM_DESC -DUSE_LTM" EXTRALIBS=-ltommath sudo make install
cd telehash-c && make
```

There will be warnings, but it should all produce a ./test and ./idgen command (for now).

### Code Components

The codebase is a set of components that can be used to create/integrate telehash services, here's a rough layout:

* `crypt*.*`: the bindings to do the crypto parts, process/generate open and line packets
* `packet.*`: main interface to a packet, includes json handling
* `hn.*`: hashname interactions
* `switch.*`: the main I/O logic of a switch, feed incoming packets to it and get channel events out
* `chan*.*`: channel interactions (primarily managed by switch)
* `bucket.*`: hashname lists, does distance sorting logic
* `path.*`: handling network path definitions (IPs, ports, etc)
