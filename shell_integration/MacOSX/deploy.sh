#!/bin/sh
# osascript $HOME/owncloud.com/client/shell_integration/MacOSX/unload.scpt

sudo rm -rf /Library/ScriptingAdditions/SyncStateFinder.osax
# Klaas' machine
OSAXDIR=$HOME/Library/Developer/Xcode/DerivedData/OwnCloud-*/Build/Products/Debug/SyncStateFinder.osax
[ -d $OSAXDIR ] ||OSAXDIR=$HOME/Library/Developer/Xcode/DerivedData/OwnCloud-*/Build/Intermediates/ArchiveIntermediates/SyncStateFinder.osax/IntermediateBuildFilesPath/UninstalledProducts/SyncStateFinder.osax

# Markus' machine
[ -d $OSAXDIR ] || echo "OSAX does not exist"
[ -d $OSAXDIR ] && sudo cp -rv $OSAXDIR /Library/ScriptingAdditions/

sudo killall Finder
sleep 1
osascript $HOME/owncloud.com/client/shell_integration/MacOSX/load.scpt
osascript $HOME/owncloud.com/client/shell_integration/MacOSX/check.scpt

