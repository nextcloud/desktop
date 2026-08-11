#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

SCRIPT_DIRECTORY=$(CDPATH= cd "$(dirname "$0")" && /bin/pwd -P)
. "${SCRIPT_DIRECTORY}/manifest.sh"

fail()
{
    printf 'Nextcloud UI Screenshots verification failed: %s\n' "$1" >&2
    exit 1
}

usage()
{
    printf 'Usage: %s DIRECTORY EXPECTED_RUN_ID\n' "$0" >&2
    printf '       %s --deliverables-only DIRECTORY\n' "$0" >&2
    exit 2
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

    if [ "${HOME_DIRECTORY}" = '/' ] || [ -L "${HOME_DIRECTORY}" ] || [ ! -d "${HOME_DIRECTORY}" ]; then
        fail "HOME is not a safe real directory: ${HOME_DIRECTORY}"
    fi
    CANONICAL_HOME=$(CDPATH= cd "${HOME_DIRECTORY}" && /bin/pwd -P)
    if [ "${CANONICAL_HOME}" != "${HOME_DIRECTORY}" ]; then
        fail "HOME does not resolve to itself: ${HOME_DIRECTORY}"
    fi
}

validate_uuid()
{
    UUID_VALUE=$1

    if [ -z "${UUID_VALUE}" ]; then
        fail 'Expected run ID is empty.'
    fi
    case "${UUID_VALUE}" in
        *'
'*) fail 'Expected run ID contains more than one line.' ;;
    esac
    if ! printf '%s\n' "${UUID_VALUE}" | LC_ALL=C /usr/bin/grep -Eq '^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$'; then
        fail "Expected run ID is not a UUID: ${UUID_VALUE}"
    fi
}

validate_verification_directory()
{
    case "${VERIFICATION_DIRECTORY}" in
        /*) ;;
        *) fail "Directory is not an absolute path: ${VERIFICATION_DIRECTORY}" ;;
    esac
    case "${VERIFICATION_DIRECTORY}" in
        *'$('*|*'${'*|*'~'*) fail "Directory contains an unresolved path expression: ${VERIFICATION_DIRECTORY}" ;;
    esac

    if [ -L "${VERIFICATION_DIRECTORY}" ]; then
        fail "Directory is a symbolic link: ${VERIFICATION_DIRECTORY}"
    fi
    if [ ! -d "${VERIFICATION_DIRECTORY}" ]; then
        fail "Directory does not exist: ${VERIFICATION_DIRECTORY}"
    fi

    CANONICAL_VERIFICATION_DIRECTORY=$(CDPATH= cd "${VERIFICATION_DIRECTORY}" && /bin/pwd -P)
    if [ "${CANONICAL_VERIFICATION_DIRECTORY}" != "${VERIFICATION_DIRECTORY}" ]; then
        fail "Directory resolves unexpectedly: ${VERIFICATION_DIRECTORY}"
    fi

    if [ "${VERIFY_MODE}" = staging ]; then
        EXPECTED_DIRECTORY="${HOME_DIRECTORY}/Library/Containers/com.nextcloud.desktopclient/Data/tmp/nextcloud-ui-screenshots"
    else
        EXPECTED_DIRECTORY="${HOME_DIRECTORY}/Downloads/Nextcloud UI Screenshots"
    fi
    if [ "${VERIFICATION_DIRECTORY}" != "${EXPECTED_DIRECTORY}" ]; then
        fail "Unexpected ${VERIFY_MODE} directory: ${VERIFICATION_DIRECTORY}"
    fi
}

require_regular_nonempty_file()
{
    REQUIRED_FILENAME=$1
    REQUIRED_PATH="${VERIFICATION_DIRECTORY}/${REQUIRED_FILENAME}"

    if [ -L "${REQUIRED_PATH}" ]; then
        fail "${REQUIRED_FILENAME} is a symbolic link."
    fi
    if [ ! -e "${REQUIRED_PATH}" ]; then
        fail "${REQUIRED_FILENAME} is missing."
    fi
    if [ ! -f "${REQUIRED_PATH}" ]; then
        fail "${REQUIRED_FILENAME} is not a regular file."
    fi
    if [ ! -s "${REQUIRED_PATH}" ]; then
        fail "${REQUIRED_FILENAME} is empty."
    fi
}

require_marker_value()
{
    MARKER_FILENAME=$1
    MARKER_EXPECTED_VALUE=$2

    if ! ui_screenshot_list_contains "${MARKER_FILENAME}" "${UI_SCREENSHOT_MARKER_FILES}"; then
        fail "${MARKER_FILENAME} is not an allowlisted marker."
    fi
    require_regular_nonempty_file "${MARKER_FILENAME}"

    MARKER_PATH="${VERIFICATION_DIRECTORY}/${MARKER_FILENAME}"
    MARKER_VALUE=$(/bin/cat "${MARKER_PATH}")
    case "${MARKER_VALUE}" in
        *'
'*) fail "${MARKER_FILENAME} contains more than one line." ;;
    esac
    if [ "${MARKER_VALUE}" != "${MARKER_EXPECTED_VALUE}" ]; then
        fail "${MARKER_FILENAME} does not match the expected run ID."
    fi

    MARKER_BYTE_COUNT=$(LC_ALL=C /usr/bin/wc -c < "${MARKER_PATH}" | /usr/bin/tr -d '[:space:]')
    MARKER_EXPECTED_BYTE_COUNT=${#MARKER_EXPECTED_VALUE}
    MARKER_EXPECTED_BYTE_COUNT_WITH_LF=$((MARKER_EXPECTED_BYTE_COUNT + 1))
    if [ "${MARKER_BYTE_COUNT}" -ne "${MARKER_EXPECTED_BYTE_COUNT}" ] \
        && [ "${MARKER_BYTE_COUNT}" -ne "${MARKER_EXPECTED_BYTE_COUNT_WITH_LF}" ]; then
        fail "${MARKER_FILENAME} contains data beyond the expected run ID."
    fi
}

reject_excluded_files()
{
    for EXCLUDED_FILENAME in ${UI_SCREENSHOT_EXCLUDED_PNG_FILES}; do
        EXCLUDED_PATH="${VERIFICATION_DIRECTORY}/${EXCLUDED_FILENAME}"
        if [ -e "${EXCLUDED_PATH}" ] || [ -L "${EXCLUDED_PATH}" ]; then
            fail "Excluded file exists: ${EXCLUDED_FILENAME}"
        fi
    done
}

reject_white_svgs()
{
    for WHITE_SVG_PATH in "${VERIFICATION_DIRECTORY}"/*-white.svg "${VERIFICATION_DIRECTORY}"/.*-white.svg; do
        if [ ! -e "${WHITE_SVG_PATH}" ] && [ ! -L "${WHITE_SVG_PATH}" ]; then
            continue
        fi
        fail "White SVG exists: ${WHITE_SVG_PATH##*/}"
    done
}

if [ "$#" -eq 2 ] && [ "$1" = '--deliverables-only' ]; then
    VERIFY_MODE=final
    VERIFICATION_DIRECTORY=$2
    EXPECTED_RUN_ID=
elif [ "$#" -eq 2 ]; then
    VERIFY_MODE=staging
    VERIFICATION_DIRECTORY=$1
    EXPECTED_RUN_ID=$2
else
    usage
fi

validate_home_directory
validate_verification_directory

if [ "${VERIFY_MODE}" = staging ]; then
    validate_uuid "${EXPECTED_RUN_ID}"
    require_marker_value '.run-id' "${EXPECTED_RUN_ID}"
    require_marker_value '.qml-complete' "${EXPECTED_RUN_ID}"
    require_marker_value '.native-complete' "${EXPECTED_RUN_ID}"
fi

for DELIVERABLE_FILENAME in ${UI_SCREENSHOT_DELIVERABLE_FILES}; do
    require_regular_nonempty_file "${DELIVERABLE_FILENAME}"
done
reject_excluded_files
reject_white_svgs

printf 'Verified %s Nextcloud UI screenshot output: %s\n' "${VERIFY_MODE}" "${VERIFICATION_DIRECTORY}"
