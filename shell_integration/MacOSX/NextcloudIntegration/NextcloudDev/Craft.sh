#!/bin/env zsh

# SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Read the available environment paths which include (for example) Homebrew.
for f in /etc/paths.d/*; do
    echo "Found to source: $f"

    while read -r line; do
        echo "Adding to PATH: $line"
        export PATH="$PATH:$line"
    done < "$f"
done

echo "Final PATH: $PATH"

if [ -f "$HOME/.zprofile" ]; then
    echo "Sourcing $HOME/.zprofile to include possible PATH definitions..."
    source "$HOME/.zprofile"
fi

DESKTOP_CLIENT_PROJECT_ROOT="$SOURCE_ROOT/../../.."

if [ -d "$DESKTOP_CLIENT_PROJECT_ROOT/admin/osx/mac-crafter" ]; then
    cd "$DESKTOP_CLIENT_PROJECT_ROOT/admin/osx/mac-crafter"
else
    echo "Error: Directory '$DESKTOP_CLIENT_PROJECT_ROOT/admin/osx/mac-crafter' does not exist!"
    exit 1
fi

# Unset so the Realm package builds: Xcode exports the outer build's index store settings into
# this script without INDEX_DATA_STORE_DIR, leaving clang a valueless -index-store-path.
unset COMPILER_INDEX_STORE_ENABLE
unset INDEX_ENABLE_DATA_STORE
unset INDEX_DATA_STORE_DIR
unset INDEX_ENABLE_OPTIMIZATION_LEVEL_OVERRIDE
unset INDEX_STORE_COMPRESS
unset INDEX_STORE_ONLY_PROJECT_FILES

# Unset so the nested xcodebuild calls keep their caches out of the source tree: Xcode 27 exports
# CCHROOT as the project directory, and xcodebuild uses it as its cache root.
unset CCHROOT

swift run mac-crafter \
    --build-path="$DESKTOP_CLIENT_PROJECT_ROOT/build" \
    --product-path="/Applications" \
    --build-type="Debug" \
    --dev \
    --disable-auto-updater \
    --build-file-provider-module \
    --code-sign-identity="Apple Development" \
    "$DESKTOP_CLIENT_PROJECT_ROOT"
