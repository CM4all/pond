Pond
====

Author: Max Kellermann <mk@cm4all.com>

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.


Configuration
-------------

The file :file:`/etc/cm4all/pond/pond.conf` configures two aspects of
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

Client
------

The package :file:`cm4all-pond-client` contains a very simple and
generic client which can be used to query logs.  Example::

  cm4all-pond-client localhost query site=foo
  cm4all-pond-client localhost query --follow

The first line queries all records of site "foo".  The second line
enables "follow" mode, which means that the client receives a
continuous live stream of records as they are received by the server,
but no past entries are shown.

The following filters are available:

- :samp:`site=NAME` shows only records of the specified site.  There
  is currently no way to filter records with no site at all.

The client displays records in the standard one-line format by
default.  If standard output is connected to a datagram or seqpacket
socket, then the log datagrams are sent in raw format instead.

Security
--------

This software implements no access restrictions.  Datagrams from
anybody are inserted into the database, and all clients are allowed to
access all data.
