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

# This script is used to build the Mac OS X version of the IONOS client.
# Parse the command line arguments
while getopts "b:p:s:civt" opt; do
  case ${opt} in
    b )BASE_DIR=$OPTARG;;
    p )PATH_TO_PKG=$OPTARG ;;
    s )CODE_SIGN_IDENTITY=$OPTARG ;;
    c )CLEAN_REBUILD=true ;;
    i )PACKAGE_INSTALLER=true ;;
    v )VERBOSE=true ;; 
    t )TEAM_PATCHING=true ;;
    \? )
      echo "Usage: sign.sh [-b <base_dir>] [-p <path_to_pkg>] [-s <code_sign_identity>] [-c clean-rebuild] [-i build-installer] [-v verbose]"
      exit 1
      ;;
  esac
done

if [ "$VERBOSE" = true ]; then
  set -xe
fi

# Some variables
PKG_FULLNAME=$(basename "$PATH_TO_PKG")
PKG_FILENAME="${PKG_FULLNAME%.pkg}"
PRODUCT_NAME="IONOS HiDrive Next"
UNDERSCORE_PRODUCT_NAME="IONOS_HiDrive_Next"

IONOS_TEAM_IDENTIFIER="5TDLCVD243"
NC_TEAM_IDENTIFIER="NKUJUXUJ3B"
INSTALLER_CERT="Developer ID Installer: $CODE_SIGN_IDENTITY"
APPLICATION_CERT="Developer ID Application: $CODE_SIGN_IDENTITY"

WORK_DIR="ex"
EXTRACTED_DIR="${BASE_DIR%/}/$WORK_DIR"

PRODUCT_DIR=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg/Payload/Applications
SCRIPTS_DIR=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg/Scripts
INNER_PKG=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg
PAYLOAD_DIR=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg/Payload
INSTALLER_PKG=$BASE_DIR/INSTALLER.pkg
APP_PATH=$PRODUCT_DIR/$PRODUCT_NAME.app

echo "Expanding original package..."

if [ -d "$EXTRACTED_DIR" ]; then

  echo "$EXTRACTED_DIR already exits."

  if [ "$CLEAN_REBUILD" = true ]; then
    echo "Clean Rebuild Enabled - Deleting folder: $EXTRACTED_DIR"
    rm -rf "$EXTRACTED_DIR"
    pkgutil --expand-full "$PATH_TO_PKG" "$EXTRACTED_DIR"
  fi
else 
  pkgutil --expand-full "$PATH_TO_PKG" "$EXTRACTED_DIR"
fi

# ---------------------------------------------------
# Patch Team Identifier 

# check wether patching is needed. ".com" is important because otherwise the ID in the signature will be found

if [[ -n "$TEAM_PATCHING" ]]; then
  PLIST_MATCHES=$(find "$APP_PATH" -name "*.plist" -exec grep -q "$NC_TEAM_IDENTIFIER.com" {} \; -print | wc -l)
  BIN_MATCHES=$(find "$APP_PATH" -type f -exec grep -q --binary-files=text "$NC_TEAM_IDENTIFIER.com" {} \; -print | wc -l)

  if [[ "$PLIST_MATCHES" -gt 0 || "$BIN_MATCHES" -gt 0 ]]; then
    # Ensure both IDs are same lengt
    if [[ ${#NC_TEAM_IDENTIFIER} -ne ${#IONOS_TEAM_IDENTIFIER} ]]; then
      echo "NC_TEAM_IDENTIFIER and IONOS_TEAM_IDENTIFIER must be the same length for binary-safe patching."
      open $BASE_DIR
      exit 1
    fi

    if [[ "$PLIST_MATCHES" -gt 0 ]]; then
    # --- Replace in .plist files (plain XML) ---
      echo "Replacing Team Identifier in .plist files..."
      find "$APP_PATH" -name "*.plist" -exec grep -q "$NC_TEAM_IDENTIFIER" {} \; -exec sed -i '' "s/$NC_TEAM_IDENTIFIER/$IONOS_TEAM_IDENTIFIER/g" {} \;
    fi
    
    if [[ "$BIN_MATCHES" -gt 0 ]]; then
      # Find and patch all binaries containing the old ID
      find "$APP_PATH" -type f -exec grep -q --binary-files=text "$NC_TEAM_IDENTIFIER" {} \; -print | while read -r file; do
        echo "Patching Team Identifier in $file"
        perl -pi -e "s/$NC_TEAM_IDENTIFIER/$IONOS_TEAM_IDENTIFIER/g" "$file"
      done
    fi
  fi
fi
# ---------------------------------------------------
# Sign the client
# CODE_SIGN_IDENTITY="Developer ID Application: IONOS SE (5TDLCVD243)"

# Check if CODE_SIGN_IDENTITY is set, if not exit
if [ -z "$CODE_SIGN_IDENTITY" ]; then
  echo "Code sign identity not set. Exiting."
  open $BASE_DIR
  exit 0
fi

echo "start signing the client"

CLIENT_CONTENTS_DIR=$APP_PATH/Contents
CLIENT_FRAMEWORKS_DIR=$CLIENT_CONTENTS_DIR/Frameworks
CLIENT_RESOURCES_DIR=$CLIENT_CONTENTS_DIR/Resources
CLIENT_PLUGINS_DIR=$CLIENT_CONTENTS_DIR/PlugIns

for script in $SCRIPTS_DIR/*; do
    echo "→ Signing script: $script"
    codesign -s "$CODE_SIGN_IDENTITY" --force --preserve-metadata=entitlements --verbose=4 --options=runtime --timestamp "$script"
done

find "$CLIENT_FRAMEWORKS_DIR" -print0 | xargs -0 -I {} bash -c 'recursive_sign "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"
find "$CLIENT_PLUGINS_DIR" -print0 | xargs -0 -I {} bash -c 'recursive_sign "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"
find "$CLIENT_RESOURCES_DIR" -print0 | xargs -0 -I {} bash -c 'recursive_sign "$@" "$CODE_SIGN_IDENTITY"' _ {} "$CODE_SIGN_IDENTITY"
codesign -s "$CODE_SIGN_IDENTITY" --force --preserve-metadata=entitlements --verbose=4 --deep --options=runtime --timestamp "$APP_PATH"

# Sign the client ---- Still needed?
find "$CLIENT_CONTENTS_DIR/MacOS" -mindepth 1 -print0 | xargs -0 -I {} bash -c 'sign_folder_content "$@" "$CODE_SIGN_IDENTITY" "$entitlements" ' _ {} "$CODE_SIGN_IDENTITY" "--preserve-metadata=entitlements"

# Validate that the key used for signing the binary matches the expected TeamIdentifier
# needed to pass the SocketApi through the sandbox for communication with virtual file system
if ! codesign -dv "$APP_PATH" 2>&1 | grep -q "TeamIdentifier=$IONOS_TEAM_IDENTIFIER"; then
  echo "TeamIdentifier does not match. Exiting."
  open $BASE_DIR
  exit 0
fi

# ---------------------------------------------------
# Installer

echo "start building the installer"

# Build the installer, if enabled
if [ -z "$PACKAGE_INSTALLER" ]; then
  echo "Installer packaging not enabled. Exiting."
  open $BASE_DIR
  exit 0
fi

echo "Renew BOM"
mkbom $PAYLOAD_DIR $INNER_PKG/Bom
echo "Reassembling the package..."
(cd $PAYLOAD_DIR && \
 find . | cpio -o --format odc | gzip -c) > $PAYLOAD_DIR.new

rm -rf $PAYLOAD_DIR
mv $PAYLOAD_DIR.new $PAYLOAD_DIR

(cd $EXTRACTED_DIR && \
  pkgutil --flatten $UNDERSCORE_PRODUCT_NAME.pkg $UNDERSCORE_PRODUCT_NAME.pkg.flat)

rm -rf $INNER_PKG
mv $INNER_PKG.flat $INNER_PKG

productsign --timestamp --sign 'Developer ID Installer: IONOS SE (5TDLCVD243)' \
  $INNER_PKG \
  $INNER_PKG.signed

rm -rf $INNER_PKG
mv $INNER_PKG.signed $INNER_PKG

(cd $BASE_DIR && productbuild \
  --distribution ex/Distribution \
  --resources ex/Resources \
  --package-path ex \
  $INSTALLER_PKG.unsigned)

productsign --timestamp --sign 'Developer ID Installer: IONOS SE (5TDLCVD243)' $INSTALLER_PKG.unsigned "$BASE_DIR$PKG_FILENAME.resigned.pkg"

# catch the output of the notarytool command
OUTPUT=$(xcrun notarytool submit --wait "$BASE_DIR$PKG_FILENAME.resigned.pkg"\
  --keychain-profile "IONOS SE HiDrive Next")

SUBMISSION_STATUS=$(echo $OUTPUT | grep -o 'status: [^ ]*' | cut -d ' ' -f 2)

# Check if the notarization was successful
if [ $SUBMISSION_STATUS != "Accepted" ]; then
  echo "Notarization failed. Exiting."
  open $BASE_DIR
  exit 1
fi

# staple
xcrun stapler staple "$BASE_DIR$PKG_FILENAME.resigned.pkg"
xcrun stapler validate "$BASE_DIR$PKG_FILENAME.resigned.pkg"

open $BASE_DIR

