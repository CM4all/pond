Pond
====

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.

For more information, `read the manual
<https://pond.readthedocs.io/en/latest/>`__ in the ``doc`` directory.


Building Pond
-------------

You need:

- a C++23 compliant compiler (e.g. GCC or clang)
- `libfmt <https://fmt.dev/>`__
- `zlib <https://www.zlib.net/>`__
- `Meson 1.2 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Optional dependencies:

- `Avahi <https://www.avahi.org/>`__
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- libgeoip

Get the source code::

 git clone --recursive https://github.com/CM4all/pond.git

Run ``meson``::

 meson setup output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us
