Pond
####

Author: Max Kellermann <mk@cm4all.com>

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.


Configuration
=============

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
    #max_age "7 days"
  }
  
  receiver {
    bind "*"
    #v6only "yes"
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

  auto_clone "yes"

The last two :samp:`listener` blocks configure local sockets, the
first one with an abstract address, and the second one with a socket
path.

Global Options
--------------

- ``auto_clone`` attempts to find another Pond server and clones its
  database.  This requires Zeroconf and will delay startup until the
  clone is complete, which can take a very long time when the database
  is large.

``database``
------------

The ``database`` block can contain the following settings:

- ``size`` specifies how much memory is allocated in total (in bytes;
  the suffixes `k`, `M`, `G` are supported).  Internally, the database
  implements a circular buffer which evicts the oldest items if there
  is no more room for another item.
- ``max_age``: if specified, then records older than this will be
  evicted even if there is still room in the buffer.
- ``per_site_message_rate_limit``: if specified, then each site is
  rate-limited to this number of messages per second.  Excess messages
  will be discarded silently.  This affects only log datagrams which
  contain only a message (e.g. :samp:`http_error`).

``receiver``
------------

The ``receiver`` block can contain the following settings:

- ``bind``: an address to bind to. May be the wildcard ``*`` or an
  IPv4/IPv6 address followed by a port. If you omit the port number,
  it will default to 5479.  IPv6 addresses should be enclosed in
  square brackets to disambiguate the port separator. Local sockets
  start with a slash :file:`/`, and abstract sockets start with the
  symbol ``@``.
- ``v6only``: if set to :samp:`yes`, then IPv4 support is disabled on
  an IPv6 listener.  This is required to avoid the port conflict when
  you need an IPv4 listener with a different configuration (e.g. an
  IPv4 multicast group).
- ``multicast_group``: join this multicast group, which allows
  receiving multicast commands. Value is a multicast IPv4/IPv6
  address.  IPv6 addresses may contain a scope identifier after a
  percent sign (``%``).
- ``interface``: limit this listener to the given network interface.

``listener``
------------

The ``listener`` block can contain the following settings:

- ``bind``: an address to bind to. May be the wildcard ``*`` or an
  IPv4/IPv6 address followed by a port. If you omit the port number,
  it will default to 5480.  IPv6 addresses should be enclosed in
  square brackets to disambiguate the port separator. Local sockets
  start with a slash :file:`/`, and abstract sockets start with the
  symbol ``@``.
- ``interface``: limit this listener to the given network interface.
- ``zeroconf_service``: if specified, then register this listener as
  Zeroconf service in the local Avahi daemon.  This can be used by
  clients to discover Pond servers.

``@include``
------------

Include another file. Example::

   @include "foo/bar.conf"
   @include_optional "foo/may-not-exist.conf"
   @include "wildcard/*.conf"

The second line silently ignores non-existing files.

The third line includes all files in the directory ``wildcard`` ending
with ``.conf``.

The specified file name may be relative to the including file.


Client
======

The package :file:`cm4all-pond-client` contains a very simple and
generic client which can be used to query logs.

Querying
--------

Example::

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
be used prefixed with ":samp:`zeroconf/`" (requires installing the
:file:`avahi-daemon` package on all servers and clients).

The following command-line options are available:

.. option:: --follow

 Follow the live stream of records as they are received by the server,
 but no past entries are shown.

.. option:: --raw

 Write raw :envvar:`LOG_RECORD` packets to standard output instead of
 pretty-printing them as text lines.

.. option:: --gzip

 Compress the output with ``gzip``.

.. option:: --geoip

 Look up all IP addresses in the GeoIP database and add a column at
 the end of each line specifying the country code (or "`-`" if the
 country is unknown).  This requires the :file:`geoip-database`
 package.

.. option:: --anonymize

 Anonymize the client IP address by zeroing a portion at the
 end.  This doesn't work in "raw" mode and doesn't affect IP addresses
 inside log messages.

.. option:: --track-visitors

 Append a "visitor id" column: each visitor is assigned a unique (and
 opaque) identification string.  This is useful in combination with
 :option:`--anonymize`, because after anonymization, visitors cannot
 be identified anymore.

.. option:: --per-site=DIRECTORY

 Instead of writing to standard output, create one file for each site
 in the specified directory.  Existing files will be skipped.

.. option:: --per-site-file=FILENAME

 Makes :option:`--per-site` create a directory for each site and
 create this file in each of them.

.. option:: --per-site-nested

 Makes :option:`--per-site` create a nested tree of directories
 instead of having one flat directory entry per site.

The following filters are available:

- :samp:`type=TYPE` shows only records of the specified type.
  Available types:

  - :samp:`http_access`: an HTTP request
  - :samp:`http_error`: an HTTP log message
  - :samp:`submission`: an email submission
  - :samp:`ssh`: a log message from an SSH server
  - :samp:`job`: a log message from a job process (e.g. `Workshop
    <https://github.com/CM4all/workshop/>`__)

- :samp:`site=NAME` shows only records of the specified site.  Specify
  an empty site name to filter records with no site at all.
- :samp:`group_site=COUNT[@SKIP]` groups all result records by their
  "site" attribute, i.e. all records with the same site will be
  returned successively, followed by all records of the next site and
  so on.  Only records for the first :envvar:`COUNT` sites are
  returned, and the rest is ignored.  The option :envvar:`SKIP`
  parameter may be used to skip a number of sites.  This can be used
  to receive records for all sites incrementally, until the result is
  empty.
- :samp:`since=ISO8601` shows only records since the given time stamp.
  See :ref:`timestamps` for details.
- :samp:`until=ISO8601` shows only records until the given time stamp.
  See :ref:`timestamps` for details.
- :samp:`time=ISO8601` is a shortcut for :samp:`since=...` and
  :samp:`until=...`
- :samp:`date=YYYY-MM-DD` is a shortcut which shows records on a
  certain date (according to the client's time zone)
- :samp:`today` is a shortcut which shows records only of today
- :samp:`status=STATUS[:END]` shows only records with the specified
  status.  If "END" is also given, then this is the open end of a
  range.  Example: :samp:`status=500:600` shows all server errors.
- :samp:`window=COUNT[@SKIP]` selects a portion (window) of the
  result.  Can limit the number of records and skip a number of
  records at the beginning.

The client displays records in the standard one-line format by
default.  If standard output is connected to a datagram or seqpacket
socket, then the log datagrams are sent in raw format instead.

.. _timestamps:

ISO8601 time stamps
^^^^^^^^^^^^^^^^^^^

Examples of accepted `ISO8601
<https://en.wikipedia.org/wiki/ISO_8601>`_ time stamps:

- :samp:`2019-02-04T16:46:41Z`
- :samp:`2019-02-04T16:46:41` (without time zone)
- :samp:`2019-02-04T16:46:41+02` (with time zone offset)
- :samp:`2019-02-04T16:46:41+0200` (with time zone offset)
- :samp:`2019-02-04T16:46:41+02:00` (with time zone offset)
- :samp:`2019-02-04T16:46` (seconds omitted)
- :samp:`2019-02-04T16` (minutes omitted)
- :samp:`2019-02-04` (time of day omitted)
- :samp:`20190204T164641` (without field separators)

Other than ISO8601, the following special tokens are understood:

- :samp:`now` is the current time stamp
- :samp:`today` is the current date in the local time zone
- :samp:`yesterday` is the previous date in the local time zone
- :samp:`tomorrow` is the next date in the local time zone

Additionally, time stamps can be specified as an offset relative to
now:

- :samp:`+30s` is in 30 seconds
- :samp:`-30s` is 30 seconds ago
- :samp:`-15` is 15 minutes ago
- :samp:`-1h` is one hour ago
- :samp:`-1d` is 24 hours ago

.. _clone:

Cloning
-------

The command :samp:`clone` can be used to clone the contents of another
Pond server::

  cm4all-pond-client @pond clone other.pond.server

This asks the local Pond server (listening on abstract socket
:file:`@pond`) to download the whole database from the Pond daemon on
host :samp:`other.pond.server`.

The operation will run asynchronously, and the client will return
immediately; during the clone, the local Pond server will not accept
any new data on its :samp:`receiver`.  It can be canceled at any time
by typing::

  cm4all-pond-client @pond cancel

This command is experimental, and should not be used for regular
operation.  It may change or be removed at any time.

Injecting Data
--------------

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
========

This software implements no access restrictions.  Datagrams from
anybody are inserted into the database, and all clients are allowed to
access all data.

Due to lack fo access restrictions, this software should not be
accessible to processes which are not authorized to see all data.
Therefore, the Pond :samp:`listener` should not be mounted into
unprivileged jails/containers; instead, `Passage
<https://github.com/CM4all/passage>`__ should be used as a bridge from
unprivileged entities to the Pond client.
