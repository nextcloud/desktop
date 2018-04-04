#!/bin/bash

# Script to create the Mac installer using the packages tool from
# http://s.sudre.free.fr/Software/Packages/about.html
#

[ "$#" -lt 2 ] && echo "Usage: create_mac_pkg.sh <CMAKE_INSTALL_DIR> <build dir> <installer sign identity>" && exit

# the path of installation must be given as parameter
if [ -z "$1" ]; then
  echo "ERROR: Provide the path to CMAKE_INSTALL_DIR to this script as first parameter."
  exit 1
fi

if [ -z "$2" ]; then
  echo "ERROR: Provide the path to build directory as second parameter."
  exit 1
fi

install_path="$1"
build_path="$2"
identity="$3"
prjfile=$build_path/admin/osx/macosx.pkgproj

# The name of the installer package
installer="@APPLICATION_SHORTNAME@-qt@Qt5Core_VERSION@-@MIRALL_VERSION_FULL@@MIRALL_VERSION_SUFFIX@"
installer_file="$installer.pkg"
installer_file_tar="$installer.pkg.tar"
installer_file_tar_bz2="$installer.pkg.tar.bz2"
installer_file_tbz="$installer.pkg.tbz"

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

# Sign the finished package if desired.
if [ ! -z "$identity" ]; then
	echo "Will try to sign the installer"
	pushd $install_path
	productsign --sign "$identity" "$installer_file" "$installer_file.new"
	mv "$installer_file".new "$installer_file"
	popd
else
	echo "No certificate given, will not sign the pkg"
fi

# FIXME: OEMs?
# they will need to do their own signing..


# Sparkle wants a tbz, it cannot install raw pkg
cd $install_path
tar cf "$installer_file_tar" "$installer_file"
bzip2 -9 "$installer_file_tar"
mv "$installer_file_tar_bz2" "$installer_file_tbz"
rc=$?
if [ $rc == 0 ]; then
  echo "Successfully created $installer_file"
else
  echo "Failed to create $installer_file"
  exit 3
fi
