#!/bin/bash

# Script to create the Mac installer using the packages tool from
# http://s.sudre.free.fr/Software/Packages/about.html
#

# the path of installation must be given as parameter
if [ -z "$1" ]; then
  echo "ERROR: Provide the CMAKE_INSTALL_DIR to this script."
  exit 1
fi

prjfile="admin/osx/macosx.pkgproj"
if [ ! -f $prjfile ]; then
  echo "ERROR: macosx.pkgproj not in admin dir, start from CMAKE_SOURCE_DIR!"
  exit 2
fi

pack="admin/ownCloud Installer.pkg"
rm -f $pack

install_path=$1

# The name of the installer package
installer=ownCloud\ Installer.pkg

# The command line tool of the "Packages" tool, see link above.
pkgbuild=/usr/local/bin/packagesbuild

$pkgbuild -F $install_path $prjfile
rc=$?

if [ $rc == 0 ]; then
  echo "Successfully created $pack"
else
  echo "Failed to create $pack"
  exit 3
fi

# FIXME: Sign the finished package.
# See http://s.sudre.free.fr/Software/documentation/Packages/en/Project_Configuration.html#5
# certname=gdbsign
# productsign --cert $certname admin/$installer ./$installer


