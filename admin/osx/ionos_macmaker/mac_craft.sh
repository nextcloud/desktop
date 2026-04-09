#!/bin/bash


# This script is used to build the Mac OS X version of the IONOS client.
# Parse the command line arguments
while getopts "b:p:s:n:k:civtu" opt; do
  case ${opt} in
    b )REL_BASE_DIR=$OPTARG;;
    p )REL_PATH_TO_PKG=$OPTARG ;;
    s )IONOS_TEAM_IDENTIFIER=$OPTARG ;;
    n )NC_TEAM_IDENTIFIER=$OPTARG ;;
    k )SPARKLE_KEY=$OPTARG ;;   # not used
    c )CLEAN_REBUILD=true ;;
    i )PACKAGE_INSTALLER=true ;;
    v )VERBOSE=true ;; 
    t )TEAM_PATCHING=true ;;
    u )BUILD_UPDATER=true ;;
    \? )
      echo "Usage: mac_craft.sh [-b <REL_BASE_DIR>] [-p <REL_PATH_TO_PKG>] [-s <IONOS_TEAM_IDENTIFIER>] [-n <NC_TEAM_IDENTIFIER>] [-k <SPARKLE_KEY>] [-c CLEAN_REBUILD] [-i PACKAGE_INSTALLER] [-v VERBOSE] [-t TEAM_PATCHING] [-u BUILD_UPDATER]"
      exit 1
      ;;
  esac
done

if [ "$VERBOSE" = true ]; then
  echo "VERBOSE MODE"
  set -xe
fi

if [ "$TEAM_PATCHING" == "true" ] && [ -z "$NC_TEAM_IDENTIFIER" ]; then
  echo "Patching aktivated, but NC_TEAM_IDENTIFIER not set. Exiting."
  exit 0
fi

# Check if CODE_SIGN_IDENTITY is set, if not exit
if [ -z "$IONOS_TEAM_IDENTIFIER" ]; then
  echo "IONOS_TEAM_IDENTIFIER not set. Exiting."
  exit 0
fi

if [ -z "$REL_BASE_DIR" ]; then
  echo "REL_BASE_DIR not set. Exiting."
  exit 0
fi

if [ -z "$REL_PATH_TO_PKG" ]; then
  echo "REL_PATH_TO_PKG not set. Exiting."
  exit 0
fi

# Some variables
BASE_DIR="$( cd "$REL_BASE_DIR" && pwd )"
PATH_TO_PKG="$( realpath "$REL_PATH_TO_PKG")"

PKG_FULLNAME=$(basename "$PATH_TO_PKG")
PKG_FILENAME="${PKG_FULLNAME%.pkg}"
PRODUCT_NAME="IONOS HiDrive Next"
UNDERSCORE_PRODUCT_NAME="IONOS_HiDrive_Next"

CODE_SIGN_IDENTITY="IONOS SE ($IONOS_TEAM_IDENTIFIER)"
INSTALLER_CERT="Developer ID Installer: $CODE_SIGN_IDENTITY"
APPLICATION_CERT="Developer ID Application: $CODE_SIGN_IDENTITY"

WORK_DIR="ex"
EXTRACTED_DIR="${BASE_DIR%/}/$WORK_DIR"

PRODUCT_DIR=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg/Payload/Applications
SCRIPTS_DIR=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg/Scripts
INNER_PKG=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg
PAYLOAD_DIR=$EXTRACTED_DIR/$UNDERSCORE_PRODUCT_NAME.pkg/Payload
INSTALLER_PKG=${BASE_DIR%/}/INSTALLER.pkg
APP_PATH=$PRODUCT_DIR/$PRODUCT_NAME.app
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ADMIN_OSX="$( cd "$SCRIPT_DIR/.." && pwd )/macosx.entitlements.cmake"
MACCRAFTER_DIR="$( cd "$SCRIPT_DIR/../mac-crafter" && pwd )"


# Sparkle Variables
PACKAGE_PATH="${BASE_DIR%/}/$PKG_FILENAME.resigned.pkg"
SPARKLE_TBZ_PATH="${PACKAGE_PATH}.tbz"
SPARKLE_DIR="${BASE_DIR%/}/sparkle"
SPARKLE_DOWNLOAD_URI="https://github.com/sparkle-project/Sparkle/releases/download/1.27.3/Sparkle-1.27.3.tar.xz"


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
  echo "Team Patching Enabled - Start Patching Detection"

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
  else
    echo "Nothing to patch"
  fi
fi

# ---------------------------------------------------
# Sign the client



echo "start signing the client"

swift run --package-path "$MACCRAFTER_DIR" \
    mac-crafter codesign \
    -c "$CODE_SIGN_IDENTITY" \
    -e "$ADMIN_OSX" \
    "$APP_PATH"

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
mkbom "$PAYLOAD_DIR" "$INNER_PKG/Bom"
echo "Reassembling the package..."
(cd "$PAYLOAD_DIR" && \
 find . | cpio -o --format odc | gzip -c) > $PAYLOAD_DIR.new

rm -rf $PAYLOAD_DIR
mv $PAYLOAD_DIR.new $PAYLOAD_DIR

(cd $EXTRACTED_DIR && \
  pkgutil --flatten $UNDERSCORE_PRODUCT_NAME.pkg $UNDERSCORE_PRODUCT_NAME.pkg.flat)

rm -rf $INNER_PKG
mv $INNER_PKG.flat $INNER_PKG

productsign --timestamp --sign "$INSTALLER_CERT" \
  $INNER_PKG \
  $INNER_PKG.signed

rm -rf $INNER_PKG
mv $INNER_PKG.signed $INNER_PKG

(cd $BASE_DIR && productbuild \
  --distribution ex/Distribution \
  --resources ex/Resources \
  --package-path ex \
  $INSTALLER_PKG.unsigned)

productsign --timestamp --sign "$INSTALLER_CERT" $INSTALLER_PKG.unsigned "$PACKAGE_PATH"

# catch the output of the notarytool command
OUTPUT=$(xcrun notarytool submit --wait "$PACKAGE_PATH"\
  --keychain-profile "IONOS SE HiDrive Next")

SUBMISSION_STATUS=$(echo $OUTPUT | grep -o 'status: [^ ]*' | cut -d ' ' -f 2)

# Check if the notarization was successful
if [ $SUBMISSION_STATUS != "Accepted" ]; then
  echo "Notarization failed. Exiting."
  open $BASE_DIR
  exit 1
fi

# staple
xcrun stapler staple "$PACKAGE_PATH"
xcrun stapler validate "$PACKAGE_PATH"


# Sparkle

SPARKLE_TBZ_PATH="${PACKAGE_PATH}.tbz"

echo "Creating Sparkle package archive: $SPARKLE_TBZ_PATH"

# Load Sparkle

if [ "$BUILD_UPDATER" == "true" ]; then
  echo "Creating Sparkle package archive: $SPARKLE_TBZ_PATH"

  if [ -d "$SPARKLE_DIR" ]; then

    echo "$SPARKLE_DIR already exits."
    echo "Deleting..."
    rm -rf "$SPARKLE_DIR"
  fi

  echo "Download Sparkle"

  mkdir -p $SPARKLE_DIR
  wget $SPARKLE_DOWNLOAD_URI -O ${SPARKLE_DIR%/}/Sparkle.tar.xz
  tar -xvf ${SPARKLE_DIR%/}/Sparkle.tar.xz -C $SPARKLE_DIR

  if tar cf "$SPARKLE_TBZ_PATH" -C "$(dirname "$PACKAGE_PATH")" "$(basename "$PACKAGE_PATH")"; then
      echo "✅ Sparkle package created successfully."
  else
      echo "❌ Could not create Sparkle package tbz!" >&2
      exit 1
  fi

  echo "Signing Sparkle package: $SPARKLE_TBZ_PATH"
  if "${SPARKLE_DIR%/}/bin/sign_update" "$SPARKLE_TBZ_PATH"; then
      echo "✅ Sparkle package signed successfully."
  else
      echo "❌ Could not sign Sparkle package tbz!" >&2
      exit 1
  fi

fi

open $BASE_DIR

