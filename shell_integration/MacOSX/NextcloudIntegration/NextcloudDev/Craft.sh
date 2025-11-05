#!/bin/env zsh

# SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Read the available environment paths which include (for example) Homebrew.
for f in /etc/paths.d/*; do
    while read -r line; do
        export PATH="$PATH:$line"
    done < "$f"
done

if [ -f "$HOME/.zprofile" ]; then
    echo "Sourcing $HOME/.zprofile to include possible PATH definitions..."
    source "$HOME/.zprofile"
fi

if [ -z "${CODE_SIGN_IDENTITY}" ]; then
    echo "Error: CODE_SIGN_IDENTITY is not defined or is empty!"
    exit 1
fi

DESKTOP_CLIENT_PROJECT_ROOT="$SOURCE_ROOT/../../.."

if [ -d "$DESKTOP_CLIENT_PROJECT_ROOT/admin/osx/mac-crafter" ]; then
    cd "$DESKTOP_CLIENT_PROJECT_ROOT/admin/osx/mac-crafter"
else
    echo "Error: Directory '$DESKTOP_CLIENT_PROJECT_ROOT/admin/osx/mac-crafter' does not exist!"
    exit 1
fi

swift run mac-crafter \
    --build-path="$SOURCE_ROOT/DerivedData" \
    --product-path="/Applications" \
    --build-type="Debug" \
    --dev \
    --disable-auto-updater \
    --build-file-provider-module \
    --code-sign-identity="$CODE_SIGN_IDENTITY" \
    "$DESKTOP_CLIENT_PROJECT_ROOT"
