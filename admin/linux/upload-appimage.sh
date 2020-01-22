#! /bin/bash

cd /build

# Upload AppImage
APPIMAGE=$(readlink -f ./Nextcloud*.AppImage)
BASENAME=$(basename ${APPIMAGE})

URL=$(curl --max-time 900 --upload-file ${APPIMAGE} https://transfer.sh/${BASENAME})
STATUS=$?

if [[ $STATUS -eq 0 ]] 
then
    echo
    echo "Get the AppImage at the link above!"
    
    curl -u $2:$3 -X POST https://api.github.com/repos/nextcloud/desktop/issues/$1/comments -d "{ \"body\" : \"AppImage: $URL<br/><br/>To test this change/fix you can simply download above AppImage, install and test it. \" }"
else
    echo
    echo "Upload failed, however this is an optional step."
fi

# Don't let the Drone build fail
exit 0
