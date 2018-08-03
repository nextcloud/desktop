#!/bin/sh

# kill the old version. see issue #2044
killall @APPLICATION_EXECUTABLE@

installer -pkg FUSE\ for\ macOS\ 3.8.1.pkg -target / -applyChoiceChangesXML settings.plist

exit 0
