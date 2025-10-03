# How to build from source

## Requirements

### Common requirements

In order to build csync, you need to install several components:

- A C compiler
- [CMake](http://www.cmake.org) >= 2.6.0.
- [check](http://check.sourceforge.net) >= 0.9.5
- [sqlite3](http://www.sqlite.org) >= 3.4
- [libneon](http://www.webdav.org/neon/) >= 0.29.0

optional:
- [libsmbclient](http://www.samba.org) >= 3.5
- [libssh](http://www.libssh.org) >= 0.5

sqlite3 is a runtime requirement. libsmbclient is needed for
the smb plugin, libssh for the sftp plugin. libneon is required for the 
ownCloud plugin.

Note that these version numbers are versions we know work correctly. If you
build and run csync successfully with an older version, please let us know.


## Building
First, you need to configure the compilation, using CMake. Go inside the
`build` dir. Create it if it doesn't exist.

GNU/Linux and MacOS X:

    cmake -DCMAKE_BUILD_TYPE=Debug ..
    make

### CMake standard options
Here is a list of the most interesting options provided out of the box by CMake.

- CMAKE_BUILD_TYPE: The type of build (can be Debug Release MinSizeRel RelWithDebInfo)
- CMAKE_INSTALL_PREFIX: The prefix to use when running make install (Default to
  /usr/local on GNU/Linux and MacOS X)
- CMAKE_C_COMPILER: The path to the C compiler
- CMAKE_CXX_COMPILER: The path to the C++ compiler

### CMake options defined for csync

Options are defined in the following files:

- DefineOptions.cmake

They can be changed with the -D option:

`cmake -DCMAKE_BUILD_TYPE=Debug -DWITH_LOG4C=OFF ..`

### Browsing/editing CMake options

In addition to passing options on the command line, you can browse and edit
CMake options using `cmakesetup` (Windows) or `ccmake` (GNU/Linux and MacOS X).

- Go to the build dir
- On Windows: run `cmakesetup`
- On GNU/Linux and MacOS X: run `ccmake ..`

## Installing

Before installing you can run the tests if everything is working:

    make test

If you want to install csync after compilation run:

    make install

## Running

The csync binary can be found in the `build/client` directory.

## About this document

This document is written using [Markdown][] syntax, making it possible to
provide usable information in both plain text and HTML format. Whenever
modifying this document please use [Markdown][] syntax.

[markdown]: http://www.daringfireball.net/projects/markdown
