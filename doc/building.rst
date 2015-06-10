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
2. Install the dependencies (as root, or using ``sudo``) using the following 
   commands for your specific Linux distribution:
  
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
the Mac OS X environment requires extra dependencies.  You can install these
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

Windows Development Build
-----------------------

If you want to test some changes and deploy them locally, you can build natively
on Windows using MinGW. If you want to generate an installer for deployment, please
follow `Windows Installer Build (Cross-Compile)`_ instead.

1. Get the required dependencies:

   * Make sure that you have CMake_ and Git_.
   * Download the Qt_ MinGW package. You will use the MinGW version bundled with it.
   * Download an `OpenSSL Windows Build`_ (the non-"Light" version)

2. Get the QtKeychain_ sources as well as the latest versions of the ownCloud client
   from Git as follows::

    git clone https://github.com/frankosterfeld/qtkeychain.git
    git clone git://github.com/owncloud/client.git

3. Open the Qt MinGW shortcut console from the Start Menu

4. Make sure that OpenSSL's ``bin`` directory as well as your qtkeychain source
   directories are in your PATH. This will allow CMake to find the library and
   headers, as well as allow the ownCloud client to find the DLLs at runtime::

    set PATH=C:\<OpenSSL Install Dir>\bin;%PATH%
    set PATH=C:\<qtkeychain Clone Dir>;%PATH%

5. Build qtkeychain **directly in the source directory** so that the DLL is built
   in the same directory as the headers to let CMake find them together through PATH::

    cd <qtkeychain Clone Dir>
    cmake -G "MinGW Makefiles" .
    mingw32-make
    cd ..

6. Create the build directory::

    mkdir client-build
    cd client-build

7. Build the client::

    cmake -G "MinGW Makefiles" ../client
    mingw32-make

  .. note:: You can try using ninja to build parallelly using
     ``cmake -G Ninja ../client`` and ``ninja`` instead.
  .. note:: Refer to the `generic build instructions`_ section for additional options.

  The owncloud binary will appear in the ``bin`` directory.

Windows Installer Build (Cross-Compile)
-----------------------

Due to the large number of dependencies, building the client installer for Windows
is **currently only officially supported on openSUSE**, by using the MinGW cross compiler.
You can set up openSUSE 13.1, 13.2 or openSUSE Factory in a virtual machine if you do not
have it installed already.

To cross-compile:

1. Add the following repositories using YaST or ``zypper ar`` (adjust when using another openSUSE version)::

    zypper ar http://download.opensuse.org/repositories/windows:/mingw/openSUSE_13.2/windows:mingw.repo
    zypper ar http://download.opensuse.org/repositories/windows:/mingw:/win32/openSUSE_13.2/windows:mingw:win32.repo

2. Install the cross-compiler packages and the cross-compiled dependencies::

    zypper install cmake make mingw32-cross-binutils mingw32-cross-cpp mingw32-cross-gcc \
                      mingw32-cross-gcc-c++ mingw32-cross-pkg-config mingw32-filesystem \
                      mingw32-headers mingw32-runtime site-config \
                      mingw32-cross-libqt5-qmake mingw32-cross-libqt5-qttools mingw32-libqt5* \
                      mingw32-cross-nsis

3. For the installer, install the NSIS installer package::

    zypper install mingw32-cross-nsis

4. Install the following plugin::

    mingw32-cross-nsis-plugin-processes mingw32-cross-nsis-plugin-uac

  .. note:: This plugin is typically required.  However, due to a current bug
     in ``mingw``, the plugins do not currently build properly from source.

5. Manually download and install the following files using ``rpm -ivh <package>``:

  .. note:: These files also work for more recent openSUSE versions!

  ::
    # RPM depends on curl for installs from HTTP
    zypper install curl

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

8. If you want to sign the installer, acquire a `Microsoft Authenticode`_ Certificate and install ``osslsigncode`` to sign the installer::

    zypper install osslsigncode

9. Sign the package::

    osslsigncode -pkcs12 $HOME/.codesign/packages.pfx -h sha1 \
               -pass yourpass \
               -n "ACME Client" \
               -i "http://acme.com" \
               -ts "http://timestamp.server/" \
               -in ${unsigned_file} \
               -out ${installer_file}

   for ``-in``, use URL to the time stamping server provided by your CA along with the Authenticode certificate. Alternatively,
   you may use the official Microsoft ``signtool`` utility on Microsoft Windows.


.. _`generic build instructions`:
Generic Build Instructions
--------------------------

Compared to previous versions, building the desktop sync client has become easier. Unlike
earlier versions, CSync, which is the sync engine library of the client, is now
part of the client source repository and not a separate module.

You can download the desktop sync client from the ownCloud `Client Download Page`_.

To build the most up to date version of the client:

1. Clone the latest versions of the client from Git_ as follows:

  ``git clone git://github.com/owncloud/client.git``
  ``git submodule init``
  ``git submodule update``

2. Create the build directory:

  ``mkdir client-build``
  ``cd client-build``

3. Configure the client build:

  ``cmake -DCMAKE_BUILD_TYPE="Debug" ../client``

  ..note:: You must use absolute paths for the ``include`` and ``library``
           directories.

  ..note:: On Mac OS X, you need to specify ``-DCMAKE_INSTALL_PREFIX=target``,
           where ``target`` is a private location, i.e. in parallel to your build
           dir by specifying ``../install``.

4. Call ``make``.

  The owncloud binary will appear in the ``bin`` directory.

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
.. _CMake: http://www.cmake.org/download
.. _CSync: http://www.csync.org
.. _`Client Download Page`: http://owncloud.org/sync-clients/
.. _Git: http://git-scm.com
.. _MacPorts: http://www.macports.org
.. _Homebrew: http://mxcl.github.com/homebrew/
.. _`OpenSSL Windows Build`: http://slproweb.com/products/Win32OpenSSL.html
.. _Qt: http://www.qt.io/download
.. _`Microsoft Authenticode`: https://msdn.microsoft.com/en-us/library/ie/ms537361%28v=vs.85%29.aspx
.. _QtKeychain: https://github.com/frankosterfeld/qtkeychain
