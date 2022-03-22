Pond
====

*Pond* is a volatile round-robin database for log messages.  It
receives log datagrams and keeps them around for a while, to allow its
clients to query them.

For more information, `read the manual
<https://pond.readthedocs.io/en/latest/>`__ in the `doc` directory.


Building Pond
-------------

You need:

- a C++17 compliant compiler (e.g. gcc or clang)
- `Boost <http://www.boost.org/>`__
- `systemd <https://www.freedesktop.org/wiki/Software/systemd/>`__
- `Meson 0.47 <http://mesonbuild.com/>`__ and `Ninja <https://ninja-build.org/>`__

Get the source code::

 git clone --recursive https://github.com/CM4all/pond.git

Run ``meson``::

 meson . output

Compile and install::

 ninja -C output
 ninja -C output install


Building the Debian package
---------------------------

After installing the build dependencies, run::

 dpkg-buildpackage -rfakeroot -b -uc -us
