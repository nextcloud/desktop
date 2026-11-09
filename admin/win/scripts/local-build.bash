#! /usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PROJECT_PATH="$SCRIPT_DIR/../../../msi-local-build"
export CRAFT_PATH="$SCRIPT_DIR/../../../../stable-35.0/windows-msvc2022_64-cl"
export Png2Ico_EXECUTABLE="$SCRIPT_DIR/../../../..//tools/png2ico.exe"
export CUSTOMIZATION_SERVICE=0
export PULL_DESKTOP=0
export CHECKOUT_DESKTOP=0
export USE_CODE_SIGNING=0;
export UPLOAD_BUILD=0

mkdir "${PROJECT_PATH}"

./build.bat && ./build-installer-collect.bat && ./build-installer-msi.bat
