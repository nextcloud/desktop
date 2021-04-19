.. _building-label:

===============================
Appendix A: Building the Client
===============================

The goal of this section is to set up a build environment for developing
and testing the Nextcloud Desktop client. If you just want to use the
Nextcloud Desktop client without developing and testing it, you should
download the latest stable build instead.

.. note:: These instructions represent a particular streamlined and easy-to-understand
   methodology, but they are by no means the only way of setting up a build
   environment.

The steps listed here have been tested multiple times and should allow you
to build the client and/or the documentation with not warnings or errors.
These instructions should be current with the version, |version|, of the
Nextcloud Client with which it ships. If you are using the most recent version
of these instructions, and you run into errors or warnings with the latest
code from the repository, please open a GitHub Issue to let us know so we can
document a workaround or fix any underlying problems.


Using GitHub
------------

By default, cloning the GitHub repository will give you the "master" branch,
which is the most recent. If for some reason you want to build an older
version of the Nextcloud Desktop client, you can choose a branch corresponding
with that version. However, for older versions of the client, please be mindful
that any issues present may have been fixed in more recent versions.

.. note:: Doing anything other than just downloading the existing code will
   require you to have a GitHub account.

If your goal in cloning and building the Nextcloud Desktop client is to
contribute to its development, and you are not already a "collaborator"
on the Nextcloud Desktop GitHub repository, you will need to create a "fork"
by clicking the "fork" button in the upper right on any GitHub page in the
repository. It is important to do this in advance because the URL for cloning
the repository is different for a fork than for the main official version.

When cloning a GitHub repository, you have two options for authenticating your
GitHub account, SSH or HTTPS. SSH requires additional setup but is more secure
and simplifies things later on. For an explanation of the differences between
HTTPS and SSH, as well as instructions to set up SSH, see this `GitHub 
help article`_ on the subject.

.. _`GitHub help article`: https://help.github.com/en/articles/which-remote-url-should-i-use

The most basic version of the Git command for cloning a repository is as follows:

.. code-block:: bash

   $ git clone <repository_url>

Which will clone the repository into the directory where you run the command.

The four versions of the ``git clone`` command are as follows:

1. HTTPS from the official repository:

   .. code-block:: bash

      $ git clone https://github.com/nextcloud/desktop.git

2. SSH from the official repository:

   .. code-block:: bash

      $ git clone git@github.com:nextcloud/desktop.git

3. HTTPS from a fork (see above):

   .. code-block:: bash

      % git clone https://github.com/<github_username>/desktop.git

4. SSH from a fork (see above):

   .. code-block:: bash

      % git clone git@github.com:<github_username>/desktop.git

macOS Development Build
-----------------------

.. note:: While it is possible to do many of the following steps using GUI
   frontends, wherever possible the Terminal commands are listed instead, in order
   to streamline the process.

1. Install Xcode from the Mac App Store:

   https://apps.apple.com/app/xcode/id497799835

Then, in Terminal:

2. Install Xcode command line tools:

   .. code-block:: bash

      % xcode-select –install

3. Install Homebrew from `brew.sh`_ (which will just give you the following):

.. _`brew.sh`: https://brew.sh

   .. code-block:: bash

      % /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

.. note:: Under certain cirumstances, you may get on error along the
   lines of ``Permission denied @ apply2files`` when installing certain
   Homebrew packages. This is `a known issue`_ and can be fixed by changing
   the permissions on the affected files with the following command:

   .. code-block:: bash

      % sudo chown -R $(whoami):admin /usr/local/* \
         && sudo chmod -R g+rwx /usr/local/*

   This workaround may lead to other shell warnings.

.. _`a known issue`: https://stackoverflow.com/a/63241724

4. Install Homebrew packages:

   .. code-block:: bash

      % brew install git qt qtkeychain cmake openssl glib cmocka

5. Certain Homebrew packages are not automatically linked in places where
   the build scripts can find them, so you can create a shell-profile script
   that will find and load them dynamically when you run a build:

   .. code-block:: bash

      % echo 'export OPENSSL_ROOT_DIR=$(brew --prefix openssl)' >> ~/.nextcloud_build_variables
      % echo 'export QT_PATH=$(brew --prefix qt5)/bin' >> ~/.nextcloud_build_variables
      % echo 'export Qt5LinguistTools_DIR=$(brew --prefix qt5)/lib/cmake/Qt5LinguistTools/' >> ~/.nextcloud_build_variables
   
   .. note:: The name ``~/.nextcloud_build_variables`` is just a suggestion for
      convenience. You can use a different file or create an entire shell
      script, but this way of doing things is the simplest to explain.

6. Clone the Nextcloud repository to a convenient location, such as ``~/Repositories``:

   .. code-block:: bash

      % mkdir ~/Repositories

   (if it doesn't already exist), then:

   .. code-block:: bash

      % cd ~/Repositories

   .. note:: The cloned repository can go basically anywhere your user account
      has write access, though it should not go in a directory synced with another
      cloud service (especially not iCloud Drive). ``~/Repositories`` is recommended
      for tidiness and consistency.

   .. code-block:: bash

      % git clone <repository_url>

   (See the above section on using GitHub for an explanation of what URL to use.)

7. Create build directory:

   .. code-block:: bash

      % cd ~/Repositories/desktop
      % mkdir build

8. Generate the build files:

.. note::

      By default Nextcloud Desktop will build in a protected directory on macOS,
      so you need to specify a build location. You can do this every time you build,
      or you can add it to your save build variables, like so:
      
   .. code-block:: bash

      % echo 'export CMAKE_INSTALL_PREFIX=~/Builds' >> ~/.nextcloud_build_variables
      
   Replace ``~/Builds`` with a different directory if you'd like the build to end up elsewhere.
   
..

   .. code-block:: bash

      % source ~/.nextcloud_build_variables
      % cd ~/Repositories/desktop/build
      % cmake ..

9. Compile and install:

   .. code-block:: bash

      % make install

Windows Development Build
-------------------------

If you want to test some changes and deploy them locally, you can build natively
on Windows using MinGW. If you want to generate an installer for deployment, please
follow `Windows Installer Build (Cross-Compile)`_ instead.

1. Get the required dependencies:

   * Make sure that you have CMake_ and Git_.
   * Download the Qt_ MinGW package. You will use the MinGW version bundled with it.
   * Download an `OpenSSL Windows Build`_ (the non-"Light" version)

2. Get the QtKeychain_ sources as well as the latest versions of the Nextcloud client
   from Git as follows

   .. code-block:: bash
   
      git clone https://github.com/frankosterfeld/qtkeychain.git
      git clone git://github.com/nextcloud/client.git

3. Open the Qt MinGW shortcut console from the Start Menu

4. Make sure that OpenSSL's ``bin`` directory as well as your qtkeychain source
   directories are in your PATH. This will allow CMake to find the library and
   headers, as well as allow the Nextcloud client to find the DLLs at runtime::

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

   .. note:: You can try using ninja to build in parallel using
      ``cmake -G Ninja ../client`` and ``ninja`` instead.
   .. note:: Refer to the :ref:`generic-build-instructions` section for additional options.

   The Nextcloud binary will appear in the ``bin`` directory.

.. _`Windows Installer Build (Cross-Compile)`:

Windows Installer (i.e. Deployment) Build (Cross-Compile)
---------------------------------------------------------

Due to the large number of dependencies, building the client installer for Windows
is **currently only officially supported on openSUSE**, by using the MinGW cross compiler.
You can set up any currently supported version of openSUSE in a virtual machine if you do not
have it installed already.

In order to make setup simple, you can use the provided Dockerfile to build your own image.

1. Assuming you are in the root of the Nextcloud Client's source tree, you can
   build an image from this Dockerfile like this::

    cd admin/win/docker
    docker build . -t nextcloud-client-win32:<version>

   Replace ``<version>`` by the version of the client you are building, e.g.
   |version| for the release of the client that this document describes.
   If you do not wish to use docker, you can run the commands in ``RUN`` manually
   in a shell, e.g. to create your own build environment in a virtual machine.

   .. note:: Docker images are specific to releases. This one refers to |version|.
             Newer releases may have different dependencies, and thus require a later
             version of the docker image! Always pick the docker image fitting your release
             of Nextcloud client!

2. From within the source tree Run the docker instance::

     docker run -v "$PWD:/home/user/client" nextcloud-client-win32:<version> \
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

Compared to previous versions, building the desktop sync client has become easier. Unlike
earlier versions, CSync, which is the sync engine library of the client, is now
part of the client source repository and not a separate module.

To build the most up-to-date version of the client:

1. Clone the latest versions of the client from Git_ as follows:

   .. code-block:: bash

      $ git clone git://github.com/nextcloud/client.git
      $ cd client
      $ git submodule update --init

2. Create the build directory

   .. code-block:: bash
   
      $ mkdir client-build
      $ cd client-build

3. Configure the client build

   .. code-block:: bash
   
      $ cmake -DCMAKE_BUILD_TYPE="Debug" ..

   .. note:: You must use absolute paths for the ``include`` and ``library``
            directories.

   .. note:: On macOS, you need to specify ``-DCMAKE_INSTALL_PREFIX=target``,
            where ``target`` is a private location, i.e. in parallel to your build
            dir by specifying ``../install``.

   .. note:: qtkeychain must be compiled with the same prefix e.g ``CMAKE_INSTALL_PREFIX=/Users/path/to/client/install/ .``

   .. note:: Example:: ``cmake -DCMAKE_PREFIX_PATH=/usr/local/opt/qt5 -DCMAKE_INSTALL_PREFIX=/Users/path/to/client/install/``

4. Call ``make``.

   The Nextcloud binary will appear in the ``bin`` directory.

5. (Optional) Call ``make install`` to install the client to the
   ``/usr/local/bin`` directory.

The following are known cmake parameters:

* ``QTKEYCHAIN_LIBRARY=/path/to/qtkeychain.dylib -DQTKEYCHAIN_INCLUDE_DIR=/path/to/qtkeychain/``:
   Used for stored credentials.  When compiling with Qt5, the library is called ``qt5keychain.dylib.``
   You need to compile QtKeychain with the same Qt version.
* ``WITH_DOC=TRUE``: Creates doc and manpages through running ``make``; also adds install statements,
  providing the ability to install using ``make install``.
* ``CMAKE_PREFIX_PATH=/path/to/Qt5.2.0/5.2.0/yourarch/lib/cmake/``: Builds using Qt5.
* ``BUILD_WITH_QT4=ON``: Builds using Qt4 (even if Qt5 is found).
* ``CMAKE_INSTALL_PREFIX=path``: Set an install prefix. This is mandatory on Mac OS

Address Sanitizer
=================

You can enable the address sanitizer to detect memory corruptions and other mistakes.
The are the following sanitizers are available:


* Address Sanitizer
* Leak anitizer
* Memory sanitizer
* Undefined sanitizer
* Threads sanitizer

You can enable one or more sanitizers through CMake. For example, to
enable the address and the undefined sanitizer, execute CMake like
``cmake .. -D ECM_ENABLE_SANITIZERS="address;undefined"``.
Keep in mind that not all combinations of sanitizers work together, and on some
platforms, not all types of sanitizers are available. For example, on Windows there is
currently only the address sanitizer available. If you are on Windows, you need to
make sure that the linker can find the sanitizer dlls at runtime. If you installed
Visual Studio in the standard location, you could find them in
**C:/ProgramFiles (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/Llvm/x64/lib/clang/10.0.0/lib/windows**.
Make sure you add this location to your path. You may also need to
`upgrade your Visual Studio version <https://docs.microsoft.com/en-us/cpp/sanitizers/asan?view=msvc-160#install-the-addresssanitizer>`_.

.. note:: If you use Visual Studio on Windows, you can enable the
          sanitizer if you click on **Manage Configurations**, scroll
          down to the section **CMake Command Arguments** and enter then
          ``-D ECM_ENABLE_SANITIZERS="address"`` in the text input field below.
          After that, click on **Save and generate CMake cache to load variables**
          right above the table.

.. _CMake: http://www.cmake.org/download
.. _CSync: http://www.csync.org
.. _Client Download Page: https://nextcloud.com/install/#install-clients
.. _Git: http://git-scm.com
.. _OpenSSL Windows Build: http://slproweb.com/products/Win32OpenSSL.html
.. _Qt: http://www.qt.io/download
.. _Microsoft Authenticode: https://msdn.microsoft.com/en-us/library/ie/ms537361%28v=vs.85%29.aspx
.. _QtKeychain: https://github.com/frankosterfeld/qtkeychain
