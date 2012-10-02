# Mirall

## Introduction

Mirall synchronizes your folders with another computer.

The ultimate goals of Mirall are:

* Network location aware: should not try to sync against your NAS if you are
  not in the home network
* It is a zero-interaction tool. So forget about resolving conflicts.
* It should work silently and realiably.

Mirall is in early stages of development, and may still eat your
files or hang your computer.

* Network location awareness not implemented yet
* Current version supports local and remote (sftp and smb) folders.
* It is powered by csync (http://www.csync.org), however
  the user does not know and other tools will be incorporated to provide other
  functionality.

## Current issues

* No sane way to backup conflicting versions yet, this should be solved
  in a near csync release (--conflictcopy, available in Jann's branch).
  Right now the newest copy wins.
* You can't remove folder configurations
  Workaround: delete ~/.local/share/data/Mirall/folders/$alias and restart
* Some tasks block the GUI (initial setup of watchers)
* May be some concurrency issues

## Roadmap

* Improve robustness to minimize user interaction
* Improve feedback and sync results
* Add support for other folder types: tarsnap, duplicity, git (SparkleShare)

## Requirements

* Linux (currently it uses inotify to detect file changes)
* unison installed in the local and remote machine
  (you should not care if you got Mirall with your favorite
   distribution)

## Download

### openSUSE

* 1-click install available in software.opensuse.org

http://software.opensuse.org/search?q=mirall&baseproject=ALL&lang=en&include_home=true&exclude_debug=true

### Source code

* https://github.com/owncloud/mirall

## Building the source code

You need Qt 4.7 and cmake:

    mkdir build
    cd build
    cmake ..
    make

To generate a tarball:

    mkdir build
    cd build
    cmake ..
    make package_source

## Authors

* Duncan Mac-Vicar P. <duncan@kde.org>
* Klaas Freitag <freitag@owncloud.com>
* Daniel Molkentin <danimo@owncloud.com>

## License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.


