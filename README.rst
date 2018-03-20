Pond
====

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.


Building Pond
-------------

You need:

- a C++14 compliant compiler (e.g. gcc or clang)
- `libevent <http://libevent.org/>`__
- `Boost <http://www.boost.org/>`__
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `Meson 0.37 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Run ``meson``::

 meson . output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us