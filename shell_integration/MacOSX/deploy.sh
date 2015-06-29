#!/bin/sh

echo "Not used anymore, please do (from build dir) (this is <= 10.9 only)"
echo sudo cp -r ./shell_integration/MacOSX/Release/SyncStateFinder.osax /Library/ScriptingAdditions/
echo killall Finder
exit 1

SELFPATH=`dirname $0`
# osascript $SELFPATH/unload.scpt

sudo rm -rf /Library/ScriptingAdditions/SyncStateFinder.osax
# Klaas' machine
OSAXDIR=$HOME/Library/Developer/Xcode/DerivedData/OwnCloud-*/Build/Products/Debug/SyncStateFinder.osax
[ -d $OSAXDIR ] ||OSAXDIR=$HOME/Library/Developer/Xcode/DerivedData/OwnCloud-*/Build/Intermediates/ArchiveIntermediates/SyncStateFinder.osax/IntermediateBuildFilesPath/UninstalledProducts/SyncStateFinder.osax

# Markus' machine
[ -d $OSAXDIR ] || echo "OSAX does not exist"
[ -d $OSAXDIR ] && sudo cp -rv $OSAXDIR /Library/ScriptingAdditions/

sudo killall Finder
sleep 1
osascript $SELFPATH/load.scpt
osascript $SELFPATH/check.scpt

