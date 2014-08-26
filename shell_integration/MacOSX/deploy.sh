#!/bin/sh
# osascript $HOME/owncloud.com/mirall/shell_integration/MacOSX/unload.scpt

sudo rm -rf /Library/ScriptingAdditions/OwnCloudFinder.osax
# Klaas' machine
OSAXDIR=$HOME/Library/Developer/Xcode/DerivedData/OwnCloud-*/Build/Products/Debug/OwnCloudFinder.osax
[ -d $OSAXDIR ] ||OSAXDIR=$HOME/Library/Developer/Xcode/DerivedData/OwnCloud-*/Build/Intermediates/ArchiveIntermediates/OwnCloudFinder.osax/IntermediateBuildFilesPath/UninstalledProducts/OwnCloudFinder.osax

# Markus' machine
[ -d $OSAXDIR ] || echo "OSAX does not exist"
[ -d $OSAXDIR ] && sudo cp -rv $OSAXDIR /Library/ScriptingAdditions/

sudo killall Finder
sleep 1
osascript $HOME/owncloud.com/mirall/shell_integration/MacOSX/load.scpt
osascript $HOME/owncloud.com/mirall/shell_integration/MacOSX/check.scpt

