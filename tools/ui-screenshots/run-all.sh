#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

SCRIPT_DIRECTORY=$(CDPATH= cd "$(dirname "$0")" && /bin/pwd -P)
. "${SCRIPT_DIRECTORY}/build-paths.sh"

fail()
{
    printf 'Nextcloud UI Screenshots: %s\n' "$1" >&2
    exit 1
}

/bin/sh "${SCRIPT_DIRECTORY}/build-screenshot-tool.sh"

if [ -L "${UI_SCREENSHOT_EXECUTABLE}" ]; then
    fail "Screenshot executable is a symbolic link: ${UI_SCREENSHOT_EXECUTABLE}"
fi
if [ ! -f "${UI_SCREENSHOT_EXECUTABLE}" ] || [ ! -x "${UI_SCREENSHOT_EXECUTABLE}" ]; then
    fail "Screenshot executable is missing or not executable: ${UI_SCREENSHOT_EXECUTABLE}"
fi

/bin/sh "${SCRIPT_DIRECTORY}/prepare.sh"

STAGING_DIRECTORY="${HOME}/Library/Containers/com.nextcloud.desktopclient/Data/tmp/nextcloud-ui-screenshots"

printf 'Starting QML screenshot phase.\n'
set +e
NEXTCLOUD_UI_SCREENSHOTS=qml \
NEXTCLOUD_UI_SCREENSHOT_OUTPUT="${STAGING_DIRECTORY}" \
    "${UI_SCREENSHOT_EXECUTABLE}"
QML_STATUS=$?
set -e
if [ "${QML_STATUS}" -ne 0 ]; then
    printf 'Nextcloud UI Screenshots: QML phase failed with exit status %s.\n' "${QML_STATUS}" >&2
    exit "${QML_STATUS}"
fi

/bin/sh "${SCRIPT_DIRECTORY}/run-native-and-export.sh"
