.. _building-label:

Appendix A: Building the Client
===============================

This section explains how to build the ownCloud Client from source for all
major platforms. You should read this section if you want to develop for the
desktop client.

.. note:: Building instruction are subject to change as development proceeds.
  Please check the version for which you want to build.

The instructions contained in this topic were updated to work with version 1.7 of the ownCloud Client.

Linux
-----

1. Add the `ownCloud repository from OBS`_.
2. Install the dependencies (as root, or using ``sudo``) using the following commands for your specific Linux distribution:

  * Debian/Ubuntu: ``apt-get update; apt-get build-dep owncloud-client``
  * openSUSE: ``zypper ref; zypper si -d owncloud-client``
  * Fedora/CentOS: ``yum install yum-utils; yum-builddep owncloud-client``

3. Follow the `generic build instructions`_.

4. (Optional) Call ``make install`` to install the client to the ``/usr/local/bin`` directory.

.. note:: This step requires the ``mingw32-cross-nsis`` packages be installed on
          Windows.

Mac OS X
--------

In additon to needing XCode (along with the command line tools), developing in
the MAC OS X environment requires extra dependencies.  You can install these
dependencies through MacPorts_ or Homebrew_.  These dependencies are required
only on the build machine, because non-standard libs are deployed in the app
bundle.

The tested and preferred way to develop in this environment is through the use
of HomeBrew_. The ownCloud team has its own repository containing non-standard
recipes.

To set up your build enviroment for development using HomeBrew_:

1. Add the ownCloud repository using the following command::

    brew tap owncloud/owncloud

2. Install any missing dependencies::

    brew install $(brew deps owncloud-client)

3. Add Qt from brew to the path::

    export PATH=/usr/local/Cellar/qt5/5.x.y/bin/qmake

   Where ``x.z`` is the current version of Qt 5 that brew has installed
   on your machine.

5. For compilation of the client, follow the `generic build instructions`_.

6. In the build directory, run ``admin/osx/create_mac.sh <build_dir>
   <install_dir>``. If you have a developer signing certificate, you can specify
   its Common Name as a third parameter (use quotes) to have the package
   signed automatically.

.. note:: Contrary to earlier versions, ownCloud 1.7 and later are packaged
          as a ``pkg`` installer. Do not call "make package" at any time when
          compiling for OS X, as this will build a disk image, and will not
          work correctly.

Windows (Cross-Compile)
-----------------------

Due to the large number of dependencies, building the client for Windows is
**currently only supported on openSUSE**, by using the MinGW cross compiler.
You can set up openSUSE 12.1, 12.2, or 13.1 in a virtual machine if you do not
have it installed already.

To cross-compile:

1. Add the following repositories using YaST or ``zypper ar`` (adjust when using openSUSE 12.2 or 13.1)::

    zypper ar http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_13.1/windows:mingw:win32.repo
    zypper ar http://download.opensuse.org/repositories/windows:/mingw/openSUSE_13.1/windows:mingw.repo

2. Install the cross-compiler packages and the cross-compiled dependencies::

    zypper install cmake make mingw32-cross-binutils mingw32-cross-cpp mingw32-cross-gcc \
                 mingw32-cross-gcc-c++ mingw32-cross-pkg-config mingw32-filesystem \
                 mingw32-headers mingw32-runtime site-config mingw32-libqt4-sql \
                 mingw32-libqt4-sql-sqlite mingw32-sqlite mingw32-libsqlite-devel \
                 mingw32-dlfcn-devel mingw32-libssh2-devel kdewin-png2ico \
                 mingw32-libqt4 mingw32-libqt4-devel mingw32-libgcrypt \
                 mingw32-libgnutls mingw32-libneon-openssl mingw32-libneon-devel \
                 mingw32-libbeecrypt mingw32-libopenssl mingw32-openssl \
                 mingw32-libpng-devel mingw32-libsqlite mingw32-qtkeychain \
                 mingw32-qtkeychain-devel mingw32-dlfcn mingw32-libintl-devel \
                 mingw32-libneon-devel mingw32-libopenssl-devel mingw32-libproxy-devel \
                 mingw32-libxml2-devel mingw32-zlib-devel

3. For the installer, install the NSIS installer package::

    zypper install mingw32-cross-nsis

4. Install the following plugin::

    mingw32-cross-nsis-plugin-processes mingw32-cross-nsis-plugin-uac

  .. note:: This plugin is typically required.  However, due to a current bug
     in ``mingw``, the plugins do not currently build properly from source.

5. Manually download and install the following files using ``rpm -ivh <package>``:

  .. note:: These files also work for more recent openSUSE versions!

  ::

    rpm -ivh http://download.tomahawk-player.org/packman/mingw:32/openSUSE_12.1/x86_64/mingw32-cross-nsis-plugin-processes-0-1.1.x86_64.rpm
    rpm -ivh http://download.tomahawk-player.org/packman/mingw:32/openSUSE_12.1/x86_64/mingw32-cross-nsis-plugin-uac-0-3.1.x86_64.rpm

6. Follow the `generic build instructions`_

  .. note:: When building for Windows platforms, you must specify a special
     toolchain file that enables cmake to locate the platform-specific tools. To add
     this parameter to the call to cmake, enter
     ``-DCMAKE_TOOLCHAIN_FILE=../client/admin/win/Toolchain-mingw32-openSUSE.cmake``.

7. Build by running ``make``.

  .. note:: Using ``make package`` produces an NSIS-based installer, provided
    the NSIS mingw32 packages are installed.

.. _`generic build instructions`:

Generic Build Instructions
--------------------------

Compared to previous versions, building Mirall has become easier. Unlike
earlier versions, CSync, which is the sync engine library of Mirall, is now
part of the Mirall source repository and not a separate module.

You can download Mirall from the ownCloud `Client Download Page`_.

To build the most up to date version of the client:

1. Clone the latest versions of Mirall from Git_ as follows:

  ``git clone git://github.com/owncloud/client.git``

2. Create build directories:

  ``mkdir client-build``

3. Build the client:

  ``cd ../client-build``
  ``cmake -DCMAKE_BUILD_TYPE="Debug" ../client``

  ..note:: You must use absolute paths for the ``include`` and ``library``
           directories.

  ..note:: On Mac OS X, you need to specify ``-DCMAKE_INSTALL_PREFIX=target``,
           where ``target`` is a private location, i.e. in parallel to your build
           dir by specifying ``../install``.

4. Call ``make``.

  The owncloud binary appear in the ``bin`` directory.

The following are known cmake parameters:

* ``QTKEYCHAIN_LIBRARY=/path/to/qtkeychain.dylib -DQTKEYCHAIN_INCLUDE_DIR=/path/to/qtkeychain/``:
   Used for stored credentials.  When compiling with Qt5, the library is called ``qt5keychain.dylib.``
   You need to compile QtKeychain with the same Qt version.
* ``WITH_DOC=TRUE``: Creates doc and manpages through running ``make``; also adds install statements,
  providing the ability to install using ``make install``.
* ``CMAKE_PREFIX_PATH=/path/to/Qt5.2.0/5.2.0/yourarch/lib/cmake/``: Builds using Qt5.
* ``BUILD_WITH_QT4=ON``: Builds using Qt4 (even if Qt5 is found).
* ``CMAKE_INSTALL_PREFIX=path``: Set an install prefix. This is mandatory on Mac OS

.. _`ownCloud repository from OBS`: http://software.opensuse.org/download/package?project=isv:ownCloud:desktop&package=owncloud-client
.. _CSync: http://www.csync.org
.. _`Client Download Page`: http://owncloud.org/sync-clients/
.. _Git: http://git-scm.com
.. _MacPorts: http://www.macports.org
.. _Homebrew: http://mxcl.github.com/homebrew/
.. _QtKeychain: https://github.com/frankosterfeld/qtkeychain
