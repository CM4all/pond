Pond
====

Author: Max Kellermann <mk@cm4all.com>

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.


Configuration
-------------

The file :file:`/etc/cm4all/pond/pond.conf` configures several aspects
of this software:

* a *receiver* is a datagram socket which binds to an address,
  optionally joins a multicast group, and receives log datagrams

* the *database* stores these datagrams

* a *listener* is a stream socket which binds to an address and
  accepts connections from clients, allowing them to query database
  contents

Example::

  database {
    size "1G"
  }
  
  receiver {
    bind "*"
    #multicast_group "ff02::dead:beef%br0"
    #interface "eth0"
  }
  
  listener {
    bind "*"
    #interface "eth0"
    #zeroconf_service "pond"
  }

The database section can specify how much memory is allocated in total
(in bytes; the suffixes `k`, `M`, `G` are supported).  Internally, the
database implements a circular buffer which evicts the oldest items if
there is no more room for another item.

The listener setting :envvar:`zeroconf_service` registers the listener
as Zeroconf service in the local Avahi daemon.  This can be used by
the client to discover Pond servers.


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

The first command-line argument specifies the Pond server to connect
to.  This can be a numeric IPv4/IPv6 address, a DNS host name, a local
socket path (starting with :samp:`/`) or an abstract socket name
(starting with :samp:`@`).  Additionally, a Zeroconf service name can
be used prefixed with ":samp:`zeroconf/`".

The following filters are available:

- :samp:`type=TYPE` shows only records of the specified type.
  Available types: :samp:`http_access` (a HTTP request),
  :samp:`http_error` (a HTTP log message), :samp:`submission` (an
  email submission)
- :samp:`site=NAME` shows only records of the specified site.  There
  is currently no way to filter records with no site at all.
- :samp:`site=NAME` shows only records of the specified site.  There
  is currently no way to filter records with no site at all.
- :samp:`group_site=COUNT[@SKIP]` limits the number of distinct sites
  in the result.  Only records for the first :envvar:`COUNT` sites are
  returned, and the rest is ignored (in order of appearance in the
  filtered list).  The option :envvar:`SKIP` parameter may be used to
  skip a number of sites.  This can be used to receive records for all
  sites incrementally, until the result is empty.
- :samp:`since=ISO8601` shows only records since the given time stamp.
- :samp:`until=ISO8601` shows only records until the given time stamp.
- :samp:`date=YYYY-MM-DD` is a shortcut which shows records on a
  certain date (according to the client's time zone)

The client displays records in the standard one-line format by
default.  If standard output is connected to a datagram or seqpacket
socket, then the log datagrams are sent in raw format instead.

Security
--------

This software implements no access restrictions.  Datagrams from
anybody are inserted into the database, and all clients are allowed to
access all data.
