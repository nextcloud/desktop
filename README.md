
# Mirall

## Introduction

Mirall synchronizes your folders with another computer. It is a zero-interaction tool. So forget about resolving conflicts. It should work silently and realiably.

* Current version supports local and remote (SSH) folders.
* It is powered by the great unison (http://www.cis.upenn.edu/~bcpierce/unison/), however
  the user does not know and other tools will be incorporated to provide other
  functionality.

Mirall is in early stages of development, and may still eat your
files or hang your computer.

## Current issues

* No sane way to backup conflicting versions yet
  Workaround: delete ~/.local/share/data/Mirall/folders/$ALIAS and restart
* You can't remove folder configurations
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

* http://github.com/dmacvicar/mirall

## Building the source code

You need:

* Qt 4.7
* cmake

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

