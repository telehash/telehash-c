telehash tools library in c
===========================

This is a growing body of code for working with telehash in c.

relies on js0n.
git clone https://github.com/quartzjer/js0n.git

## Usage Overview

This codebase is designed to be used/embedded into other applications and devices and all of the portable code is located in the [lib](lib/) folder, but that doesn't include any crypto or networking since those are platform specific.  The unix-system utilities (basic crypto, filesystem, and sockets) are in the [unix](unix/) folder, and some example apps that use them are in the [util](util/) folder. There's also some experimental arduino code in the [avr](avr/) folder.  There is a lot of portable support code for handling all the different channel types in the [ext](ext/) folder, some or many of which are required for basic functionality.

The core "switch" library is purely event driven by pushing raw packets from the network into it, and reading packets from active channels back out of it.  A good example of this pattern is in the [field test](util/tft.c) utility, and it's a bit more to consider when integrating, but it allows ultimate control and flexibility in handling all of the event processing.

### Code Components

The codebase is a set of components that can be used to create/integrate telehash services, here's a rough layout:

* `crypt*.*`: the bindings to do the crypto parts, process/generate open and line packets
* `packet.*`: main interface to a packet, includes json handling
* `hn.*`: hashname interactions
* `switch.*`: the main I/O logic of a switch, feed incoming packets to it and get channel events out
* `chan*.*`: channel interactions (primarily managed by switch)
* `bucket.*`: hashname lists, does distance sorting logic
* `path.*`: handling network path definitions (IPs, ports, etc)
