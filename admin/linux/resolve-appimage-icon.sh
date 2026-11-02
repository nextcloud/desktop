#!/bin/bash

# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

if [ "$#" -ne 1 ]; then
    printf 'Usage: %s desktop-file\n' "$0" >&2
    exit 1
fi

desktop_file=$1
icon_name=$(awk '
    { sub(/\r$/, "") }
    /^\[Desktop Entry\]$/ { in_entry = 1; next }
    /^\[/ { in_entry = 0 }
    in_entry && /^Icon=/ { print substr($0, 6); exit }
' "$desktop_file")

if [ -z "$icon_name" ]; then
    printf 'No Icon entry found in %s\n' "$desktop_file" >&2
    exit 1
fi

icon_file="$(dirname -- "$(dirname -- "$desktop_file")")/icons/hicolor/512x512/apps/${icon_name}.png"
if [ ! -f "$icon_file" ]; then
    printf 'AppImage icon does not exist: %s\n' "$icon_file" >&2
    exit 1
fi

printf '%s\n' "$icon_file"
