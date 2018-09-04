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

  listener {
    bind "@pond"
  }

  listener {
    bind "/run/cm4all/pond/socket"
  }

The database section can specify how much memory is allocated in total
(in bytes; the suffixes `k`, `M`, `G` are supported).  Internally, the
database implements a circular buffer which evicts the oldest items if
there is no more room for another item.

The listener setting :envvar:`zeroconf_service` registers the listener
as Zeroconf service in the local Avahi daemon.  This can be used by
the client to discover Pond servers.

The last two :samp:`listener` blocks configure local sockets, the
first one with an abstract address, and the second one with a socket
path.


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
be used prefixed with ":samp:`zeroconf/`" (requires
:file:`avahi-daemon` on both client and server).

The following command-line options are available:

.. option:: --follow

 Follow the live stream of records as they are received by the server,
 but no past entries are shown.

.. option:: --raw

 Write raw :envvar:`LOG_RECORD` packets to standard output instead of
 pretty-printing them as text lines.

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
- :samp:`today` is a shortcut which shows records only of today

The client displays records in the standard one-line format by
default.  If standard output is connected to a datagram or seqpacket
socket, then the log datagrams are sent in raw format instead.

.. _clone:

Cloning
^^^^^^^

The command :samp:`clone` can be used to clone the contents of another
Pond server::

  cm4all-pond-client @pond clone other.pond.server

This asks the local Pond server (listening on abstract socket
:file:`@pond`) to download the whole database from the Pond daemon on
host :samp:`other.pond.server`.  The operation will block until the
clone has completed; during that time, the local Pond server will not
accept any new data on its :samp:`receiver`.

This command is experimental, and should not be used for regular
operation.  It may change or be removed at any time.

Injecting Data
^^^^^^^^^^^^^^

The command :samp:`inject` reads :envvar:`LOG_RECORD` packets from
standard input (possibly generated with :option:`--raw`) and inject
them into the Pond server.  The server will only allow this if the
client is local (connected with a local socket, not TCP) and
privileged.  Example::

  cm4all-pond-client pond.server.local query --raw ... |
    cm4all-pond-client @pond inject

This example shows something that is similar to :ref:`clone`, but less
efficient, because all data now passes through the client, while
:samp:`clone` transfers data directly between the two Pond servers.

This command was implemented for development and debugging, and is not
meant for production use.


Security
--------

This software implements no access restrictions.  Datagrams from
anybody are inserted into the database, and all clients are allowed to
access all data.
