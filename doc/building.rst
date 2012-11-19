Building the Client
===================

This section explains how to build the ownCloud Client from source
for all major platforms. You should read this section if you want
to development on the desktop client.

Linux
-----

1. Add the `ownCloud repository from OBS`_.
2. Install the dependencies (as root, or via sudo):

  * Debian/Ubuntu: ``apt-get update; apt-get build-dep owncloud-client``
  * openSUSE: ``zypper ref; zypper si -d owncloud-client``
  * Fedora/CentOS: ``yum install yum-utils; yum-builddep owncloud-client``

3. Follow the `generic build instructions`_.

Mac OS X
--------

Follow the `generic build instructions`_.
 
You can install the missing dependencies via MacPorts_ or Homebrew_.
This is only needed on the build machine, since non-standard libs
will be deployed in the app bundle.

The only exception to this rule is libiniparser_, which lacks a decent
build system. If you are using Homebrew_, you can just add it::

  brew tap dschmidt/owncloud
  brew install iniparser

Otherwise, you need to copy the header and lib files to
``/usr/local/include`` and ``/usr/local/lib`` respectively. 

.. note::
  You should not call ``make install`` at any time, since the product of the
  mirall build is an app bundle. Call ``make package`` instead to create an
  install-ready disk image.

Windows (cross-compile)
-----------------------

Due to the amount of dependencies that csync entails, building the client
for Windows is **currently only supported on openSUSE**, by using the MinGW
cross compiler. You can set up openSUSE 12.1 or 12.2 in a virtual machine
if you do not have it installed already.

In order to cross-compile, the following repositories need to be added
via YaST or ``zypper ar`` (adjust when using openSUSE 12.2)::

  http://download.opensuse.org/repositories/isv:/ownCloud:/devel:/mingw:/win32/openSUSE_12.1/isv:ownCloud:devel:mingw:win32.repo
  http://download.opensuse.org/repositories/windows:/mingw/openSUSE_12.1/windows:mingw.repo
  http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_12.1/windows:mingw:win32.repo

Next, install the cross-compiler packages and the cross-compiled dependencies::

  zypper si -d mingw32-csync
  zypper install kdewin-png2ico mingw32-libqt4 mingw32-libqt4-devel

For the installer, the NSIS installer packages are also required::

  zypper install mingw32-cross-nsis mingw32-cross-nsis-plugin-processes \
                 mingw32-cross-nsis-plugin-uac

Now, follow the `generic build instructions`_, but make sure to append
the following parameter to both cmake calls::

  -DCMAKE_TOOLCHAIN_FILE=../mirall/admin/win/Toolchain-mingw32-openSUSE.cmake

Finally, just build by running ``make``. ``make package`` will produce
an NSIS-based installer, provided the NSIS mingw32 packages are installed.

Generic Build Instructions
--------------------------
.. _`generic build instructions`

The ownCloud Client requires Mirall and CSync_. Mirall is the GUI frontend,
while CSync is responsible for handling the actual synchronization process.

At the moment, ownCloud Client requires a forked version of CSync. Both
CMake and Mirall can be downloaded at ownCloud's `Client Download Page`_.

If you want to build the leading edge version of the client, you should
use the latest versions of Mirall and CSync via Git_, like so::

  git clone git://git.csync.org/users/freitag/csync.git ocsync
  git clone git://github.com/owncloud/mirall.git

Next, create build directories::

  mkdir ocsync-build
  mkdir mirall-build

This guide assumes that all directories are residing next to each other.
Next, make sure to check out the 'dav' branch in the newly checked out
`ocsync` directory::

  cd ocsync
  git checkout dav

The first package to buidld is CSync::

  cd ocsync-build
  cmake -DCMAKE_BUILD_TYPE="Debug" -DLOG_TO_CALLBACK=ON -DWITH_LOG4C=OFF ../ocsync
  make

You probably have to satisfy some dependencies. Make sure to install all the
needed development packages. You will need ``iniparser``, ``sqlite3`` as well as
``neon`` for the ownCloud module. ``libssh`` and ``libsmbclient`` are optional
and not required for the client to work. If you want to install the client, run
``make install`` as a final step.

Next, we build mirall::

  cd ../mirall-build
  cmake -DCMAKE_BUILD_TYPE="Debug" ../mirall \
        -DCSYNC_LIBRARY_PATH=../ocsync-build \
        -DCSYNC_INCLUDE_PATH=../ocsync/src

If this succeeds, call ``make``. The owncloud binary should appear in the
``bin`` directory. You can also run ``make install`` to install the client to
``/usr/local/bin``.

To build in installer (requires the mingw32-cross-nsis packages)::

  make package

.. _`ownCloud repository from OBS`: http://software.opensuse.org/download/package?project=isv:ownCloud:devel&package=owncloud-client
.. _CSync: http://www.csync.org
.. _`Client Download Page`: http://owncloud.org/sync-clients/
.. _Git: http://git-scm.com
.. _MacPorts: http://www.macports.org
.. _Homebrew: http://mxcl.github.com/homebrew/
.. _libiniparser: http://ndevilla.free.fr/iniparser/
