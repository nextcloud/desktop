#!/bin/bash
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="$repo_root/build/macos-clang-$(uname -m)/build/nextcloud-client/work/build"
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
    while IFS= read -r candidate; do
        if [[ "$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$candidate")" == "$repo_root" ]]; then
            build_dir="$(dirname "$candidate")"
            break
        fi
    done < <(find "$repo_root/build" -name CMakeCache.txt -print 2>/dev/null)
fi
cache="$build_dir/CMakeCache.txt"

if [[ ! -f "$cache" ]]; then
    printf 'No configured client build found under %s/build. Build NextcloudDev from this checkout first.\n' "$repo_root" >&2
    exit 1
fi

configured_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
if [[ "$configured_source" != "$repo_root" ]]; then
    printf 'The selected build belongs to a different source directory: %s\n' "$configured_source" >&2
    exit 1
fi

if [[ "${1:-}" == --print-build-dir ]]; then
    printf '%s\n' "$build_dir"
    exit 0
fi

cmake_command="$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "$cache")"
if [[ ! -x "$cmake_command" ]]; then
    printf 'The configured CMake executable is unavailable: %s\n' "$cmake_command" >&2
    exit 1
fi

exec "$cmake_command" --build "$build_dir" --target DocAssetsCapture --parallel
