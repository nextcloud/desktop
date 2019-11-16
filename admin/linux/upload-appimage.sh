#! /bin/bash

set -xe

cd /build

# Upload AppImage
APPIMAGE=$(readlink -f ./Nextcloud*.AppImage)
BASENAME=$(basename ${APPIMAGE})

if curl --max-time 900 --upload-file ${APPIMAGE} https://transfer.sh/${BASENAME}
then
    echo
    echo "Get the AppImage at the link above!"
else
    echo
    echo "Upload failed, however this is an optional step."
fi

# Don't let the Drone build fail
exit 0