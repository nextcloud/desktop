#!/bin/bash

# Script to create the Mac installer using the packages tool from
# http://s.sudre.free.fr/Software/Packages/about.html
#

# the path of installation must be given as parameter
if [ -z "$1" ]; then
  echo "ERROR: Provide the path to CMAKE_INSTALL_DIR to this script as first parameter."
  exit 1
fi

if [ -z "$2" ]; then
  echo "ERROR: Provide the path to build directory as second parameter."
  exit 1
fi

install_path=$1
build_path=$2
prjfile=$build_path/admin/osx/macosx.pkgproj

# The name of the installer package
installer="ownCloud-@MIRALL_VERSION_FULL@@MIRALL_VERSION_SUFFIX@"
installer_file="$installer.pkg"

# set the installer name to the copied prj config file
/usr/local/bin/packagesutil --file $prjfile set project name "$installer"

# The command line tool of the "Packages" tool, see link above.
pkgbuild=/usr/local/bin/packagesbuild

$pkgbuild -F $install_path $prjfile
rc=$?

if [ $rc == 0 ]; then
  echo "Successfully created $installer_file"
else
  echo "Failed to create $installer_file"
  exit 3
fi

# FIXME: Sign the finished package.
# See http://s.sudre.free.fr/Software/documentation/Packages/en/Project_Configuration.html#5
# certname=gdbsign
# productsign --cert $certname admin/$installer ./$installer

# FIXME: OEMs?
