.. _building-label:

===============================
Appendix A: Building the Client
===============================

This section explains how to build the ownCloud Client from source for all
major platforms. You should read this section if you want to develop for the
desktop client.

.. note:: Build instructions are subject to change as development proceeds.
  Please check the version for which you want to build.

These instructions are updated to work with version |version| of the ownCloud Client.


Compiling via ownBrander
------------------------

If you don't want to go through the trouble of doing all the compile work manually,
you can use `ownBrander`_ to create installer images for all platforms.


Getting Source Code
-------------------

The :ref:`generic-build-instructions` pull the latest code directly from 
GitHub, and work on Linux, Mac OS X, and Windows.

See the next section for instructions on getting source code from Linux 
packages.

Linux
-----

For the published desktop clients we link against qt5 dependencies from our ouwn repositories, os that we can have the same versions on all distributions. This chapter shows you how to build the client yourself with this setup.
If you want to use the qt5 dependencies from your system, see the next chapter.

You may wish to use source packages for your Linux distribution, as these give 
you the exact sources from which the binary packages are built. These are 
hosted on the `ownCloud repository from OBS`_. Go to the `Index of 
repositories`_ to see all the Linux client repos.

1. The source RPMs for CentOS, RHEL, Fedora, SLES, and openSUSE are at the `bottom of the page for each distribution 
   <https://software.opensuse.org/download/package?project=isv:ownCloud:desktop&
   package=owncloud-client>`
   the sources for DEB and Ubuntu based distributions are at e.g. http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/Ubuntu_18.04/
   
   To get the .deb source packages add the source 
   repo for your Debian or Ubuntu version, like this example for Debian 9 
   (run as root)::
 
    echo 'deb 
    http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/Debian_9.0/ /' >> /etc/apt/sources.list.d/owncloud-client.list
    echo 'deb-src 
    http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/Debian_9.0/ /' >> /etc/apt/sources.list.d/owncloud-client.list

2. Install the dependencies using the following commands for your specific Linux 
distribution. Make sure the repositories for source packages are enabled.
  
   * Debian/Ubuntu: ``apt update; apt build-dep owncloud-client``
   * openSUSE/SLES: ``zypper ref; zypper si -d owncloud-client``
   * Fedora/CentOS/RHEL: ``yum install yum-utils; yum-builddep owncloud-client``

3. Follow the :ref:`generic-build-instructions`, starting with step 2.

Linux with system dependencies
------------------------------
1. Build sources from e.g. a github checkout with dependencies provided by your linux distribution. While this allows more freedom for development, it does not exactly represent what we ship as packages. See above for how to recreate packages from source.

  * Debian/Ubuntu: ``apt install qtdeclarative5-dev libinotifytools-dev qt5keychain-dev libqt5webkit5-dev python-sphinx libsqlite3-dev``

2. Follow the :ref:`generic-build-instructions`, starting with step 1.

macOS
-----

In addition to needing XCode (along with the command line tools), developing in
the Mac OS X environment requires extra dependencies.  You can install these
dependencies through MacPorts_ or Homebrew_.  These dependencies are required
only on the build machine, because non-standard libs are deployed in the app
bundle.

The tested and preferred way to develop in this environment is through the use
of HomeBrew_. The ownCloud team has its own repository containing non-standard
recipes.

To set up your build environment for development using HomeBrew_:

1. Install Xcode
2. Install Xcode command line tools::
    xcode-select --install

3. Install homebrew::
    /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

4. Add the ownCloud repository using the following command::

    brew tap owncloud/owncloud

5. Install a Qt5 version, ideally from from 5.10.1::

    brew install qt5

6. Install any missing dependencies::

    brew install $(brew deps owncloud-client)

7. Install qtkeychain from here:  git clone https://github.com/frankosterfeld/qtkeychain.git
   make sure you make the same install prefix as later while building the client e.g.
   ``-DCMAKE_INSTALL_PREFIX=/Path/to/client/../install``

8. For compilation of the client, follow the :ref:`generic-build-instructions`.

9. Install the Packages_ package creation tool.

10. In the build directory, run ``admin/osx/create_mac.sh <CMAKE_INSTALL_DIR> <build dir> <installer sign identity>``.
    If you have a developer signing certificate, you can specify
    its Common Name as a third parameter (use quotes) to have the package
    signed automatically.

   .. note:: Contrary to earlier versions, ownCloud 1.7 and later are packaged
             as a ``pkg`` installer. Do not call "make package" at any time when
             compiling for OS X, as this will build a disk image, and will not
             work correctly.

Windows Development Build
-------------------------

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

     cmake -G "MinGW Makefiles" -DNO_SHIBBOLETH=1 ../client
     mingw32-make

   .. note:: You can try using ninja to build in parallel using
      ``cmake -G Ninja ../client`` and ``ninja`` instead.
   .. note:: Refer to the :ref:`generic-build-instructions` section for additional options.

   The ownCloud binary will appear in the ``bin`` directory.

Windows Installer Build (Cross-Compile)
---------------------------------------

Due to the large number of dependencies, building the client installer for Windows
is **currently only officially supported on openSUSE**, by using the MinGW cross compiler.
You can set up any currently supported version of openSUSE in a virtual machine if you do not
have it installed already.

In order to make setup simple, you can use the provided Dockerfile to build your own image. 

1. Assuming you are in the root of the ownCloud Client's source tree, you can
   build an image from this Dockerfile like this::

    cd admin/win/docker
    docker build . -t owncloud-client-win32:<version>

   Replace ``<version>`` by the version of the client you are building, e.g.
   |version| for the release of the client that this document describes.
   If you do not wish to use docker, you can run the commands in ``RUN`` manually
   in a shell, e.g. to create your own build environment in a virtual machine.

   .. note:: Docker images are specific to releases. This one refers to |version|.
             Newer releases may have different dependencies, and thus require a later
             version of the docker image! Always pick the docker image fitting your release
             of ownCloud client!

2. From within the source tree Run the docker instance::

     docker run -v "$PWD:/home/user/client" owncloud-client-win32:<version> \
        /home/user/client/admin/win/docker/build.sh client/  $(id -u)

   It will run the build, create an NSIS based installer, as well as run tests.
   You will find the resulting binary in an newly created ``build-win32`` subfolder.

   If you do not wish to use docker, and ran the ``RUN`` commands above in a virtual machine,
   you can run the indented commands in the lower section of ``build.sh`` manually in your
   source tree.

4. Finally, you should sign the installer to avoid warnings upon installation.
   This requires a `Microsoft Authenticode`_ Certificate ``osslsigncode`` to sign the installer::

     osslsigncode -pkcs12 $HOME/.codesign/packages.pfx -h sha256 \
               -pass yourpass \
               -n "ACME Client" \
               -i "http://acme.com" \
               -ts "http://timestamp.server/" \
               -in ${unsigned_file} \
               -out ${installer_file}

   For ``-in``, use the URL to the time stamping server provided by your CA along with the Authenticode certificate. Alternatively,
   you may use the official Microsoft ``signtool`` utility on Microsoft Windows.

   If you're familiar with docker, you can use the version of ``osslsigncode`` that is part of the docker image.

.. _generic-build-instructions:

Generic Build Instructions
--------------------------

To build the most up-to-date version of the client:

1. Clone the latest versions of the client from Git_ as follows::

     git clone git://github.com/owncloud/client.git
     cd client
     # master this default, but you can also check out a tag like v2.4.1
     git checkout master
     git submodule init
     git submodule update

2. Create the build directory::

     mkdir client-build
     cd client-build

3. Configure the client build::

     cmake -DCMAKE_PREFIX_PATH=/opt/ownCloud/qt-5.10.1 -DCMAKE_INSTALL_PREFIX=/Users/path/to/client/../install/  -DNO_SHIBBOLETH=1 ..

.. note:: For Linux builds (using QT5 libraries via build-dep) a typical setting is ``-DCMAKE_PREFIX_PATH=/opt/ownCloud/qt-5.10.1/`` - version number may vary. For Linux builds using system dependencies -DCMAKE_PREFIX_PATH is not needed.

.. note:: You must use absolute paths for the ``include`` and ``library``
         directories.

.. note:: On Mac OS X, you need to specify ``-DCMAKE_INSTALL_PREFIX=target``,
         where ``target`` is a private location, i.e. in parallel to your build
         dir by specifying ``../install``.

.. note:: qtkeychain must be compiled with the same prefix e.g ``-DCMAKE_INSTALL_PREFIX=/Users/path/to/client/../install/``


4. Call ``make``.

   The owncloud binary will appear in the ``bin`` directory.
   
5. (Optional) Call ``make install`` to install the client to the   
   ``/usr/local/bin`` directory.   

The following are known cmake parameters:

* ``QTKEYCHAIN_LIBRARY=/path/to/qtkeychain.dylib -DQTKEYCHAIN_INCLUDE_DIR=/path/to/qtkeychain/``:
   Used for stored credentials.  When compiling with Qt5, the library is called ``qt5keychain.dylib.``
   You need to compile QtKeychain with the same Qt version. If you install QtKeychain into the CMAKE_PREFIX_PATH then you don't need to specify the path manually.
* ``WITH_DOC=TRUE``: Creates doc and manpages through running ``make``; also adds install statements,
  providing the ability to install using ``make install``.
* ``CMAKE_PREFIX_PATH=/path/to/Qt5.10.1/5.10.1/yourarch/lib/cmake/``: Builds using that Qt version.
* ``CMAKE_INSTALL_PREFIX=path``: Set an install prefix. This is mandatory on Mac OS

.. _ownCloud repository from OBS: http://software.opensuse.org/download/package? 
   project=isv:ownCloud:desktop&package=owncloud-client
.. _CMake: http://www.cmake.org/download
.. _CSync: http://www.csync.org
.. _Client Download Page: https://owncloud.org/install/#desktop
.. _Git: http://git-scm.com
.. _MacPorts: http://www.macports.org
.. _Homebrew: http://mxcl.github.com/homebrew/
.. _OpenSSL Windows Build: http://slproweb.com/products/Win32OpenSSL.html
.. _Qt: http://www.qt.io/download
.. _Microsoft Authenticode: https://msdn.microsoft.com/en-us/library/ie/ms537361%28v=vs.85%29.aspx
.. _QtKeychain: https://github.com/frankosterfeld/qtkeychain
.. _Packages: http://s.sudre.free.fr/Software/Packages/about.html
.. _Index of repositories: http://download.opensuse.org/repositories/isv:/ownCloud:/desktop/
.. _ownBrander: https://doc.owncloud.org/branded_clients/
