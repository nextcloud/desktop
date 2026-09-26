#!/bin/bash
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
client_executable="/Applications/NextcloudDev.app/Contents/MacOS/NextcloudDev"
if [[ $# -ne 0 ]]; then
    printf 'Run this script without arguments. Assets are saved in Downloads.\n' >&2
    exit 2
fi
if [[ ! -x "$client_executable" ]]; then
    printf 'NextcloudDev is missing from /Applications.\n' >&2
    exit 1
fi
if pgrep -x 'Nextcloud|NextcloudDev|nextcloud' >/dev/null 2>&1; then
    printf 'Quit Nextcloud before capturing. The saved NextcloudDev login will be reused.\n' >&2
    exit 1
fi
printf 'Using the saved NextcloudDev account. It must be connected to your test server.\n'
mkdir -p "$HOME/Downloads"
capture_log="$(mktemp "$HOME/Downloads/Nextcloud-doc-assets-log-XXXXXX")"
printf 'Capture log: %s\n' "$capture_log"
exec > >(tee -a "$capture_log") 2>&1
"$repo_root/test/docassets/compile.sh"
build_dir="$("$repo_root/test/docassets/compile.sh" --print-build-dir)"
capture_executable="$build_dir/bin/DocAssetsCapture.app/Contents/MacOS/DocAssetsCapture"
"$capture_executable" --check-platform
exec "$capture_executable"
