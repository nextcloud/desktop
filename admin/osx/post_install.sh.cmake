#!/bin/sh

osascript << EOF
tell application "Finder"
   activate
   select the last Finder window
	reveal POSIX file "/Applications/@APPLICATION_EXECUTABLE@.app"
end tell
EOF

exit 0