#!/bin/bash

recursive_sign(){
  local path="$1"
  local extension="${path##*.}"
  if [[ "$extension" == "dylib" || "$extension" == "framework" || "$extension" == "appex" ]]; then
    echo "Signing directory: $path"
    codesign -s "$2" --force --preserve-metadata=entitlements --verbose=4 --deep --options=runtime --timestamp "${path}" 
  fi
}

export -f recursive_sign

sign_folder_content(){
  local folder="$1"
  local identity="$2"
  local entitlements="$3"
  codesign -s "$identity" --force $entitlements --verbose=4 --deep --options=runtime --timestamp "${folder}" 
}

export -f sign_folder_content

launch_app() {
  export CRAFT="$HOME/Craft64"
  export DYLD_LIBRARY_PATH="$CRAFT/lib"
  export DYLD_FRAMEWORK_PATH="$CRAFT/lib"
  export QT_PLUGIN_PATH="$CRAFT/plugins"
  export QT_QPA_PLATFORM_PLUGIN_PATH="$CRAFT/plugins/platforms"
  export QML2_IMPORT_PATH="$CRAFT/qml"
  "$BUILD_DIR/bin/IONOS HiDrive Next.app/Contents/MacOS/IONOS HiDrive Next"
}

# This script is used to build the Mac OS X version of the IONOS client.
set -xe

# Parse the command line arguments
while getopts "a:b:s:cifoumr" opt; do
  case ${opt} in
    a ) ARCHITECTURE=$OPTARG ;;
    b ) BUILD_DIR=$OPTARG ;;
    s ) CODE_SIGN_IDENTITY=$OPTARG ;;
    c ) CLEAN_REBUILD=true ;;
    i ) PACKAGE_INSTALLER=true ;;
    f ) BUILD_FILEPROVIDER=true ;;
    o ) OSX_BUNDLE=true ;;
    u ) BUILD_UPDATER=true ;;
    m ) SKIP_MACDEPLOY=true ;;
    r ) RUN_AFTER_BUILD=true ;;
    \? )
      echo "Usage: start.sh -a <architecture> -b <build_dir> [-s <code_sign_identity>] [-c] [-f] [-i] [-m] [-o] [-r] [-u]"
      exit 1
      ;;
  esac
done

# Set the deployment target
export MACOSX_DEPLOYMENT_TARGET=13.0
# export MACOSX_DEPLOYMENT_TARGET=10.15

# Some variables
# The product name depends on whether we build an OSX bundle or not.
# See src/gui/CMakeLists.txt: OUTPUT_NAME is set to APPLICATION_NAME (bundle) or APPLICATION_EXECUTABLE (non-bundle).
if [ "$OSX_BUNDLE" == "true" ]; then
  PRODUCT_NAME="IONOS HiDrive Next"
else
  PRODUCT_NAME="ionos-hidrive-next"
fi
REPO_ROOT_DIR="../../.."
CRAFT_DIR=~/Craft64
PRODUCT_DIR=$BUILD_DIR/product
TEAM_IDENTIFIER="5TDLCVD243"

# Check if the client is running and kill it
# This is necessary to avoid issues with replacement of the bundle file
if pgrep -x "$PRODUCT_NAME" >/dev/null; then
  killall "$PRODUCT_NAME"
fi

# Check if BUILD_DIR is set, so we don't accidentally delete the whole filesystem
if [ -z "$BUILD_DIR" ]; then
  echo "Build dir not set. Add -b <build_dir> to the command."
  exit 0
fi

# Check if ARCHITECTURE is set
if [ -z "$ARCHITECTURE" ]; then
  echo "ARCHITECTURE not set. Add -a <ARCHITECTURE> to the command."
  exit 0
fi

# Check if BUILD_DIR exists. If not, create it. If so, clear it.
if [ ! -d $BUILD_DIR ]; then
  mkdir -p $BUILD_DIR
else
  if [ $CLEAN_REBUILD = true ]; then
    rm -rf $BUILD_DIR/*
  fi
fi

# Check if Craft dir exists, if not exit
if [ ! -d $CRAFT_DIR ]; then
  echo "Craft dir not found. Exiting."
  exit 1
fi

# Load Sparkle
SPARKLE_DIR=$BUILD_DIR/sparkle
SPARKLE_DOWNLOAD_URI="https://github.com/sparkle-project/Sparkle/releases/download/1.27.3/Sparkle-1.27.3.tar.xz"

if [ "$CLEAN_REBUILD" == "true" ] && [ "$BUILD_UPDATER" == "true" ]; then
  mkdir -p $SPARKLE_DIR
  wget $SPARKLE_DOWNLOAD_URI -O $SPARKLE_DIR/Sparkle.tar.xz
  tar -xvf $SPARKLE_DIR/Sparkle.tar.xz -C $SPARKLE_DIR

  # Sign Sparkle
  if [ -n "$CODE_SIGN_IDENTITY" ]; then
    SPARKLE_FRAMEWORK_DIR=$SPARKLE_DIR/Sparkle.framework
    find "$SPARKLE_FRAMEWORK_DIR/Resources/Autoupdate.app/Contents/MacOS" -mindepth 1 -print0 | xargs -0 -I {} bash -c 'sign_folder_content "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"
    codesign -s "$CODE_SIGN_IDENTITY" --force --preserve-metadata=entitlements --verbose=4 --deep --options=runtime --timestamp "$SPARKLE_FRAMEWORK_DIR/Sparkle"
  fi
fi

# Nach Zeile ~133, vor dem eigentlichen Build-Befehl (ninja/cmake --build)
# QML-Ressourcen invalidieren damit Änderungen erkannt werden
# find "$BUILD_DIR" -name "*.qmlc" -delete
# find "$BUILD_DIR" -name "qrc_*.cpp" -delete

# Build the client
# Only reconfigure if this is a clean build or no CMakeCache exists yet
# if [ "$CLEAN_REBUILD" == "true" ] || [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  cmake -S $REPO_ROOT_DIR/ -B $BUILD_DIR \
        -G Ninja \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DQT_TRANSLATIONS_DIR=$REPO_ROOT_DIR/translations \
        -DCMAKE_INSTALL_PREFIX=$PRODUCT_DIR \
        -DBUILD_TESTING=OFF \
      -DBUILD_UPDATER=$(if [ $BUILD_UPDATER == true ]; then echo "ON"; else echo "OFF"; fi) \
        -DMIRALL_VERSION_BUILD=`date +%Y%m%d` \
        -DMIRALL_VERSION_SUFFIX="stable" \
      -DBUILD_OWNCLOUD_OSX_BUNDLE=$(if [ $OSX_BUNDLE == true ]; then echo "ON"; else echo "OFF"; fi) \
        -DCMAKE_OSX_ARCHITECTURES=$ARCHITECTURE \
      -DBUILD_FILE_PROVIDER_MODULE=$(if [ $BUILD_FILEPROVIDER == true ]; then echo "ON"; else echo "OFF"; fi) \
        -DCMAKE_PREFIX_PATH=$CRAFT_DIR \
        -DSPARKLE_LIBRARY=$SPARKLE_DIR/Sparkle.framework \
        -DSOCKETAPI_TEAM_IDENTIFIER_PREFIX="$TEAM_IDENTIFIER." \
        -DARG_SIDEBAR_ICONS=ON \
        -DLOCALBUILD=ON \
      -DSKIP_MACDEPLOYQT=$(if [ $SKIP_MACDEPLOY == true ]; then echo "ON"; else echo "OFF"; fi) \
        -DWHITELABEL_NAME=ionos \
      -DDO_NOT_USE_PROXY=ON

# fi
ninja -C $BUILD_DIR install -v

# ---------------------------------------------------
# Sign the client
# CODE_SIGN_IDENTITY="Developer ID Application: IONOS SE (5TDLCVD243)"

# Check if CODE_SIGN_IDENTITY is set, if not skip signing
if [ -z "$CODE_SIGN_IDENTITY" ]; then
  echo "Code sign identity not set. Skipping signing."
  open $PRODUCT_DIR
  [ "$RUN_AFTER_BUILD" == "true" ] && launch_app
  exit 0
fi

PRODUCT_PATH=$PRODUCT_DIR/$PRODUCT_NAME.app

CLIENT_CONTENTS_DIR=$PRODUCT_PATH/Contents
CLIENT_FRAMEWORKS_DIR=$CLIENT_CONTENTS_DIR/Frameworks
CLIENT_PLUGINS_DIR=$CLIENT_CONTENTS_DIR/PlugIns
CLIENT_RESOURCES_DIR=$CLIENT_CONTENTS_DIR/Resources

find "$CLIENT_FRAMEWORKS_DIR" -print0 | xargs -0 -I {} bash -c 'recursive_sign "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"
find "$CLIENT_PLUGINS_DIR" -print0 | xargs -0 -I {} bash -c 'recursive_sign "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"
find "$CLIENT_RESOURCES_DIR" -print0 | xargs -0 -I {} bash -c 'recursive_sign "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"

codesign -s "$CODE_SIGN_IDENTITY" --force --preserve-metadata=entitlements --verbose=0 --deep --options=runtime "$PRODUCT_PATH"


# Sign the client
find "$CLIENT_CONTENTS_DIR/MacOS" -mindepth 1 -print0 | xargs -0 -I {} bash -c 'sign_folder_content "$@" "$CODE_SIGN_IDENTITY" "$entitlements" ' _ {} "$CODE_SIGN_IDENTITY" "--preserve-metadata=entitlements"

# Validate that the key used for signing the binary matches the expected TeamIdentifier
# needed to pass the SocketApi through the sandbox for communication with virtual file system
if ! codesign -dv "$PRODUCT_PATH" 2>&1 | grep -q "TeamIdentifier=$TEAM_IDENTIFIER"; then
  echo "TeamIdentifier does not match. Exiting."
  exit 0
fi

# ---------------------------------------------------
# Installer

# Build the installer, if enabled
if [ -z "$PACKAGE_INSTALLER" ]; then
  echo "Installer packaging not enabled. Exiting."
  open $PRODUCT_DIR
  [ "$RUN_AFTER_BUILD" == "true" ] && launch_app
  exit 0
fi

# package
$BUILD_DIR/admin/osx/create_mac.sh "$PRODUCT_DIR" "$BUILD_DIR" 'Developer ID Installer: IONOS SE (5TDLCVD243)'

# notariaze
# Extract package filename from filesystem per .pkg extension
PACKAGE_FILENAME=$(ls $PRODUCT_DIR/*.pkg)

# catch the output of the notarytool command
OUTPUT=$(xcrun notarytool submit --wait $PACKAGE_FILENAME\
  --keychain-profile "IONOS SE HiDrive Next")

SUBMISSION_STATUS=$(echo $OUTPUT | grep -o 'status: [^ ]*' | cut -d ' ' -f 2)

# Check if the notarization was successful
if [ $SUBMISSION_STATUS != "Accepted" ]; then
  echo "Notarization failed. Exiting."
  exit 1
fi

# staple
xcrun stapler staple $PACKAGE_FILENAME
xcrun stapler validate $PACKAGE_FILENAME

open $PRODUCT_DIR
[ "$RUN_AFTER_BUILD" == "true" ] && launch_app
