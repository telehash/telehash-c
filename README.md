telehash tools library in c
===========================

This is a growing set of tools for working with telehash in c.

relies on js0n.
git clone https://github.com/quartzjer/js0n.git

### Code Components

The codebase is a set of components that can be used to create/integrate telehash services, here's a rough layout:

* `crypt*.*`: the bindings to do the crypto parts, process/generate open and line packets
* `packet.*`: main interface to a packet, includes json handling
* `hn.*`: hashname interactions
* `switch.*`: the main I/O logic of a switch, feed incoming packets to it and get channel events out
* `chan*.*`: channel interactions (primarily managed by switch)
* `bucket.*`: hashname lists, does distance sorting logic
* `path.*`: handling network path definitions (IPs, ports, etc)
