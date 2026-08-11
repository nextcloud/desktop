#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

SCRIPT_DIRECTORY=$(CDPATH= cd "$(dirname "$0")" && /bin/pwd -P)
. "${SCRIPT_DIRECTORY}/manifest.sh"

fail()
{
    printf 'Nextcloud UI Screenshots: %s\n' "$1" >&2
    exit 1
}

validate_home_directory()
{
    if [ -z "${HOME-}" ]; then
        fail 'HOME is empty.'
    fi

    HOME_DIRECTORY=${HOME%/}
    case "${HOME_DIRECTORY}" in
        /*) ;;
        *) fail "HOME is not an absolute path: ${HOME}" ;;
    esac
    case "${HOME_DIRECTORY}" in
        *'$('*|*'${'*|*'~'*) fail "HOME contains an unresolved path expression: ${HOME_DIRECTORY}" ;;
    esac

    if [ "${HOME_DIRECTORY}" = '/' ]; then
        fail 'HOME must not be the filesystem root.'
    fi
    if [ -L "${HOME_DIRECTORY}" ]; then
        fail "HOME is a symbolic link: ${HOME_DIRECTORY}"
    fi
    if [ ! -d "${HOME_DIRECTORY}" ]; then
        fail "HOME is not a directory: ${HOME_DIRECTORY}"
    fi

    CANONICAL_HOME=$(CDPATH= cd "${HOME_DIRECTORY}" && /bin/pwd -P)
    if [ "${CANONICAL_HOME}" != "${HOME_DIRECTORY}" ]; then
        fail "HOME does not resolve to itself: ${HOME_DIRECTORY}"
    fi
}

ensure_fixed_directory()
{
    FIXED_DIRECTORY=$1

    if [ -L "${FIXED_DIRECTORY}" ]; then
        fail "Directory is a symbolic link: ${FIXED_DIRECTORY}"
    fi
    if [ -e "${FIXED_DIRECTORY}" ]; then
        if [ ! -d "${FIXED_DIRECTORY}" ]; then
            fail "Expected a directory: ${FIXED_DIRECTORY}"
        fi
        return
    fi

    /bin/mkdir "${FIXED_DIRECTORY}" || fail "Could not create directory: ${FIXED_DIRECTORY}"
    if [ -L "${FIXED_DIRECTORY}" ] || [ ! -d "${FIXED_DIRECTORY}" ]; then
        fail "Created path is not a safe directory: ${FIXED_DIRECTORY}"
    fi
}

reject_conflicting_clients()
{
    for CLIENT_PROCESS_NAME in Nextcloud NextcloudDev nextcloud-ui-screenshots; do
        if /usr/bin/pgrep -x "${CLIENT_PROCESS_NAME}" >/dev/null 2>&1; then
            fail "A conflicting ${CLIENT_PROCESS_NAME} process is running. Quit it manually; this script will not terminate it."
        fi
    done
}

remove_allowlisted_staging_file()
{
    STAGING_FILENAME=$1

    if ! ui_screenshot_list_contains "${STAGING_FILENAME}" "${UI_SCREENSHOT_STAGING_CLEANUP_FILES}"; then
        fail "Refusing to remove a filename outside the staging allowlist: ${STAGING_FILENAME}"
    fi
    case "${STAGING_FILENAME}" in
        ''|*/*|.|..) fail "Invalid staging cleanup filename: ${STAGING_FILENAME}" ;;
    esac

    STAGING_TARGET="${STAGING_DIRECTORY}/${STAGING_FILENAME}"
    if [ -L "${STAGING_TARGET}" ]; then
        fail "Refusing to remove a symbolic link from staging: ${STAGING_FILENAME}"
    fi
    if [ ! -e "${STAGING_TARGET}" ]; then
        return
    fi
    if [ ! -f "${STAGING_TARGET}" ]; then
        fail "Refusing to remove a non-file staging target: ${STAGING_FILENAME}"
    fi

    printf 'Removing stale staging file: %s\n' "${STAGING_FILENAME}"
    /bin/rm -f "${STAGING_TARGET}"
}

reject_remaining_white_svgs()
{
    for WHITE_SVG_PATH in "${STAGING_DIRECTORY}"/*-white.svg "${STAGING_DIRECTORY}"/.*-white.svg; do
        if [ ! -e "${WHITE_SVG_PATH}" ] && [ ! -L "${WHITE_SVG_PATH}" ]; then
            continue
        fi
        fail "Unexpected white SVG remains in staging: ${WHITE_SVG_PATH##*/}"
    done
}

validate_home_directory
reject_conflicting_clients

APP_CONTAINER_DIRECTORY="${HOME_DIRECTORY}/Library/Containers/com.nextcloud.desktopclient"
APP_CONTAINER_DATA_DIRECTORY="${APP_CONTAINER_DIRECTORY}/Data"
APP_CONTAINER_TMP_DIRECTORY="${APP_CONTAINER_DATA_DIRECTORY}/tmp"
STAGING_DIRECTORY="${APP_CONTAINER_TMP_DIRECTORY}/nextcloud-ui-screenshots"

# Create only this fixed path, one checked component at a time, so mkdir never
# traverses an unchecked symbolic link.
ensure_fixed_directory "${HOME_DIRECTORY}/Library"
ensure_fixed_directory "${HOME_DIRECTORY}/Library/Containers"
ensure_fixed_directory "${APP_CONTAINER_DIRECTORY}"
ensure_fixed_directory "${APP_CONTAINER_DATA_DIRECTORY}"
ensure_fixed_directory "${APP_CONTAINER_TMP_DIRECTORY}"
ensure_fixed_directory "${STAGING_DIRECTORY}"

CANONICAL_TMP_DIRECTORY=$(CDPATH= cd "${APP_CONTAINER_TMP_DIRECTORY}" && /bin/pwd -P)
CANONICAL_STAGING_DIRECTORY=$(CDPATH= cd "${STAGING_DIRECTORY}" && /bin/pwd -P)
if [ "${CANONICAL_TMP_DIRECTORY}" != "${APP_CONTAINER_TMP_DIRECTORY}" ]; then
    fail "The app-container temporary directory resolves unexpectedly: ${APP_CONTAINER_TMP_DIRECTORY}"
fi
if [ "${CANONICAL_STAGING_DIRECTORY}" != "${APP_CONTAINER_TMP_DIRECTORY}/nextcloud-ui-screenshots" ]; then
    fail "The staging directory is outside the expected app-container temporary directory: ${STAGING_DIRECTORY}"
fi

for STAGING_CLEANUP_NAME in ${UI_SCREENSHOT_STAGING_CLEANUP_FILES}; do
    remove_allowlisted_staging_file "${STAGING_CLEANUP_NAME}"
done
reject_remaining_white_svgs

printf 'Prepared Nextcloud UI screenshot staging directory: %s\n' "${STAGING_DIRECTORY}"
