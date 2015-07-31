#!/bin/sh

osascript << EOF
tell application "Finder"
   activate
   select the last Finder window
	reveal POSIX file "/Applications/@APPLICATION_EXECUTABLE@.app"
end tell
EOF

# Always enable the new 10.10 finder plugin if available
if [ -x "$(command -v pluginkit)" ]; then
    pluginkit -e use -i @APPLICATION_REV_DOMAIN@.FinderSyncExt
fi

exit 0