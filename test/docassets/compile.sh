#!/bin/bash
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
build_dir="${DOC_ASSETS_BUILD_DIR:-$repo_root/build/macos-clang-$(uname -m)/build/nextcloud-client/work/build}"
cache="$build_dir/CMakeCache.txt"

if [[ ! -f "$cache" ]]; then
    printf 'Missing configured client build: %s\nSet DOC_ASSETS_BUILD_DIR to the existing CMake build directory.\n' "$build_dir" >&2
    exit 1
fi

configured_source="$(sed -n 's/^CMAKE_HOME_DIRECTORY:INTERNAL=//p' "$cache")"
if [[ "$configured_source" != "$repo_root" ]]; then
    printf 'The selected build belongs to a different source directory: %s\n' "$configured_source" >&2
    exit 1
fi

cmake_command="$(sed -n 's/^CMAKE_COMMAND:INTERNAL=//p' "$cache")"
if [[ ! -x "$cmake_command" ]]; then
    printf 'The configured CMake executable is unavailable: %s\n' "$cmake_command" >&2
    exit 1
fi

exec "$cmake_command" --build "$build_dir" --target DocAssetsCapture --parallel
