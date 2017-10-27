Pond
====

Author: Max Kellermann <mk@cm4all.com>

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.


Configuration
-------------

The file :file:`/etc/cm4all/pond/pong.conf` configures two aspects of
this software: receivers and listeners.

* a *receiver* is a datagram socket which binds to an address,
  optionally joins a multicast group, and receives log datagrams

* a *listener* is a stream socket which binds to an address and
  accepts connections from clients, allowing them to query database
  contents

Example::

  receiver {
    bind "*"
    #multicast_group "ff02::dead:beef%br0"
    #interface "eth0"
  }
  
  listener {
    bind "*"
    #interface "eth0"
  }


Security
--------

This software implements no access restrictions.  Datagrams from
anybody are inserted into the database, and all clients are allowed to
access all data.
