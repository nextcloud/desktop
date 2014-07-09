#!/bin/sh
# osascript $HOME/owncloud.com/mirall/shell_integration/MacOSX/unload.scpt

sudo rm -rf /Library/ScriptingAdditions/LiferayNativity.osax
sudo cp -r $HOME/Library/Developer/Xcode/DerivedData/LiferayNativity-flulyntyemvwtcgiriaddqfewwvc/Build/Products/Debug/LiferayNativity.osax /Library/ScriptingAdditions/

sudo killall Finder
sleep 1
osascript $HOME/owncloud.com/mirall/shell_integration/MacOSX/load.scpt
osascript $HOME/owncloud.com/mirall/shell_integration/MacOSX/check.scpt

