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

swift run mac-crafter \
    --build-path="$DESKTOP_CLIENT_PROJECT_ROOT/.mac-crafter" \
    --product-path="/Applications" \
    --build-type="Debug" \
    --dev \
    --disable-auto-updater \
    --build-file-provider-module \
    --code-sign-identity="Apple Development" \
    "$DESKTOP_CLIENT_PROJECT_ROOT"
