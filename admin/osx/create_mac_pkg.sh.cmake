#!/bin/bash

# Script to create the Mac installer using the packages tool from
# http://s.sudre.free.fr/Software/Packages/about.html
#

# the path of installation must be given as parameter
if [ -z "$1" ]; then
  echo "ERROR: Provide the CMAKE_INSTALL_DIR to this script."
  exit 1
fi

prjfile=macosx.pkgproj
vanilla_prjfile="@CMAKE_SOURCE_DIR@/admin/osx/macosx.pkgproj"
if [ ! -f $vanilla_prjfile ]; then
  echo "ERROR: macosx.pkgproj not in admin dir, start from CMAKE_SOURCE_DIR!"
  exit 2
fi

cp $vanilla_prjfile $prjfile

install_path=$1

# The name of the installer package
installer=ownCloud-@MIRALL_VERSION_STRING@
installer_file=$installer.pkg

# set the installer name to the copied prj config file
/usr/local/bin/packagesutil --file $prjfile set project name $installer

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
