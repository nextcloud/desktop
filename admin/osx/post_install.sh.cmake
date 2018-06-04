#!/bin/sh

# Check if Finder is running (for systems with Finder disabled)
finder_status=`ps aux | grep "/System/Library/CoreServices/Finder.app/Contents/MacOS/Finder" | grep -v "grep"`
if ! [ "$finder_status" == "" ] ; then # Finder is running
   osascript << EOF
tell application "Finder"
   activate
   select the last Finder window
	reveal POSIX file "/Applications/@APPLICATION_EXECUTABLE@.app"
end tell
EOF
fi

# Always enable the new 10.10 finder plugin if available
if [ -x "$(command -v pluginkit)" ]; then
    # add it to DB. This happens automatically too but we try to push it a bit harder for issue #3463
    pluginkit -a  "/Applications/@APPLICATION_EXECUTABLE@.app/Contents/PlugIns/FinderSyncExt.appex/"
    # Since El Capitan we need to sleep #4650
    sleep 10s
    # enable it
    pluginkit -e use -i @APPLICATION_REV_DOMAIN@.FinderSyncExt
fi

exit 0
