#! /bin/bash

# Env
export BUILD=${DRONE_BUILD_NUMBER}
export PR=${DRONE_PULL_REQUEST}
export GIT_USERNAME=${CI_UPLOAD_GIT_USERNAME}
export GIT_TOKEN=${CI_UPLOAD_GIT_TOKEN}

# Needed to get it working on drone
export SUFFIX=${DRONE_PULL_REQUEST:=master}
if [ $SUFFIX != "master" ]; then
    SUFFIX="PR-$SUFFIX"
fi
export DESKTOP_CLIENT_ROOT=${DESKTOP_CLIENT_ROOT:-/home/user}
export APPNAME=${APPNAME:-nextcloud}

# Defaults
export GIT_REPO=ci-builds
export API_BASE_URL=https://api.github.com/repos/$GIT_USERNAME/$GIT_REPO
export DESKTOP_API_BASE_URL=https://api.github.com/repos/nextcloud/desktop

# PR / master
export TAG_NAME=${PR:=master}
export RELEASE_BODY=https://github.com/nextcloud/desktop

if [ $TAG_NAME != "master" ]; then
    TAG_NAME="PR-$TAG_NAME"
    RELEASE_BODY="nextcloud/desktop#$PR"
fi

cd ${DESKTOP_CLIENT_ROOT}
echo `pwd`
ls

# AppImage
if [ ! -z "$DRONE_COMMIT" ]
then
    export APPIMAGE=$(readlink -f ./${APPNAME}-${SUFFIX}-${DRONE_COMMIT}-x86_64.AppImage)
else
    export APPIMAGE=$(readlink -f ./Nextcloud*.AppImage)
fi

export UPDATE=$(readlink -f ./Nextcloud*.AppImage.zsync)
export BASENAME=$(basename ${APPIMAGE})

if ! test -e $APPIMAGE ; then
    exit 1
fi

echo "Found AppImage: $BASENAME"

if [ $TAG_NAME != "master" ]; then
    # Delete all old comments in desktop PR, starting with "AppImage file:"
    oldComments=$(curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X GET $DESKTOP_API_BASE_URL/issues/$PR/comments | jq '.[] | (.id |tostring) + "|" + (.user.login | test("'${GIT_USERNAME}'") | tostring) + "|" + (.body | test("AppImage file:.*") | tostring)'  | grep "true|true" | tr -d "\"" | cut -f1 -d"|")

    if [[ "$oldComments" != "" ]]; then
        echo $oldComments | while read comment ; do
            curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X DELETE $DESKTOP_API_BASE_URL/issues/comments/$comment
        done
    fi
fi

# Helper functions
urldecode() { : "${*//+/ }"; echo -e "${_//%/\\x}"; }

create_release()
{
    name=$TAG_NAME
    body=$RELEASE_BODY
    tagName=$TAG_NAME
    echo $(curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X POST $API_BASE_URL/releases -d "{ \"tag_name\": \"$tagName\", \"target_commitish\": \"master\", \"name\": \"$name\", \"body\": \"$body\", \"draft\": false, \"prerelease\": true }")
}

get_release()
{
    tagName=$TAG_NAME
    echo $(curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X GET $API_BASE_URL/releases/tags/$tagName)
}

get_release_assets()
{
    releaseId=$1
    echo $(curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X GET $API_BASE_URL/releases/$releaseId/assets)
}

upload_release_asset()
{
    uploadUrl=$1
    echo $(curl --max-time 900 -u $GIT_USERNAME:$GIT_TOKEN -X POST $uploadUrl --header "Content-Type: application/octet-stream" --upload-file $APPIMAGE)
    echo $(curl --max-time 900 -u $GIT_USERNAME:$GIT_TOKEN -X POST $uploadUrl --header "Content-Type: application/octet-stream" --upload-file $UPDATE)
}

delete_release_asset()
{
    assetId=$1
    curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X DELETE $API_BASE_URL/releases/assets/$assetId
}

# Try to get an already existing release
json=$(get_release)

releaseId=$(echo $json | jq -r '.id')
uploadUrl=$(echo $json | jq -r '.upload_url')

if [[ "$uploadUrl" == "null" ]]; then
    # Try to create a release
    json=$(create_release)
    echo $json

    releaseId=$(echo $json | jq -r '.id')
    uploadUrl=$(echo $json | jq -r '.upload_url')

    if [[ "$uploadUrl" == "null" ]]; then
        echo "create_release failed: $json"
        exit 2
    fi
fi

# Prepare upload url
uploadUrl=$(echo "${uploadUrl/'{?name,label}'/?name=$BASENAME}")

# Try to delete existing AppImage assets for this PR
assets=$(get_release_assets $releaseId)

for data in $(echo $assets | jq -r '.[] | @uri'); do
    json=$(urldecode "$data")

    assetId=$(echo $json | jq -r '.id')
    name=$(echo $json | jq -r '.name')

    if [[ "$name" == *.AppImage ]]; then
        echo "Deleting old asset: $name"
        $(delete_release_asset $assetId)
    fi
done

# Upload release asset
echo "Uploading new asset: $BASENAME"

json=$(upload_release_asset "$uploadUrl")
browserDownloadUrl=$(echo $json | jq -r '.browser_download_url')

if [[ "$browserDownloadUrl" == "null" ]]; then
    echo "upload_release_asset failed: $json"
    exit 3
fi

if [ $TAG_NAME != "master" ]; then
    # Create comment in desktop PR
    curl 2>/dev/null -u $GIT_USERNAME:$GIT_TOKEN -X POST $DESKTOP_API_BASE_URL/issues/$PR/comments -d "{ \"body\" : \"AppImage file: [$BASENAME]($browserDownloadUrl) <br/><br/>To test this change/fix you can simply download above AppImage file and test it. <br/><br/>Please make sure to quit your existing Nextcloud app and backup your data. \" }"
fi

echo
echo "AppImage link: $browserDownloadUrl"
