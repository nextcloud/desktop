#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

set -eu

SCRIPT_DIRECTORY=$(CDPATH= cd "$(dirname "$0")" && /bin/pwd -P)
. "${SCRIPT_DIRECTORY}/manifest.sh"
. "${SCRIPT_DIRECTORY}/build-paths.sh"

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

    if [ "${HOME_DIRECTORY}" = '/' ] || [ -L "${HOME_DIRECTORY}" ] || [ ! -d "${HOME_DIRECTORY}" ]; then
        fail "HOME is not a safe real directory: ${HOME_DIRECTORY}"
    fi
    CANONICAL_HOME=$(CDPATH= cd "${HOME_DIRECTORY}" && /bin/pwd -P)
    if [ "${CANONICAL_HOME}" != "${HOME_DIRECTORY}" ]; then
        fail "HOME does not resolve to itself: ${HOME_DIRECTORY}"
    fi
}

require_fixed_directory()
{
    FIXED_DIRECTORY=$1

    if [ -L "${FIXED_DIRECTORY}" ]; then
        fail "Directory is a symbolic link: ${FIXED_DIRECTORY}"
    fi
    if [ ! -d "${FIXED_DIRECTORY}" ]; then
        fail "Required directory does not exist: ${FIXED_DIRECTORY}"
    fi
}

validate_uuid()
{
    UUID_VALUE=$1

    if [ -z "${UUID_VALUE}" ]; then
        fail 'Run ID is empty.'
    fi
    case "${UUID_VALUE}" in
        *'
'*) fail 'Run ID contains more than one line.' ;;
    esac
    if ! printf '%s\n' "${UUID_VALUE}" | LC_ALL=C /usr/bin/grep -Eq '^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$'; then
        fail "Run ID is not a UUID: ${UUID_VALUE}"
    fi
}

require_regular_nonempty_staging_file()
{
    REQUIRED_FILENAME=$1
    REQUIRED_PATH="${STAGING_DIRECTORY}/${REQUIRED_FILENAME}"

    if [ -L "${REQUIRED_PATH}" ]; then
        fail "Staging file is a symbolic link: ${REQUIRED_FILENAME}"
    fi
    if [ ! -e "${REQUIRED_PATH}" ]; then
        fail "Staging file is missing: ${REQUIRED_FILENAME}"
    fi
    if [ ! -f "${REQUIRED_PATH}" ]; then
        fail "Staging path is not a regular file: ${REQUIRED_FILENAME}"
    fi
    if [ ! -s "${REQUIRED_PATH}" ]; then
        fail "Staging file is empty: ${REQUIRED_FILENAME}"
    fi
}

read_run_id()
{
    require_regular_nonempty_staging_file '.run-id'
    RUN_ID=$(/bin/cat "${STAGING_DIRECTORY}/.run-id")
    validate_uuid "${RUN_ID}"

    RUN_ID_BYTE_COUNT=$(LC_ALL=C /usr/bin/wc -c < "${STAGING_DIRECTORY}/.run-id" | /usr/bin/tr -d '[:space:]')
    RUN_ID_EXPECTED_BYTE_COUNT=${#RUN_ID}
    RUN_ID_EXPECTED_BYTE_COUNT_WITH_LF=$((RUN_ID_EXPECTED_BYTE_COUNT + 1))
    if [ "${RUN_ID_BYTE_COUNT}" -ne "${RUN_ID_EXPECTED_BYTE_COUNT}" ] \
        && [ "${RUN_ID_BYTE_COUNT}" -ne "${RUN_ID_EXPECTED_BYTE_COUNT_WITH_LF}" ]; then
        fail '.run-id contains data beyond its UUID.'
    fi
}

require_matching_marker()
{
    MARKER_FILENAME=$1

    if ! ui_screenshot_list_contains "${MARKER_FILENAME}" "${UI_SCREENSHOT_MARKER_FILES}"; then
        fail "Marker is not allowlisted: ${MARKER_FILENAME}"
    fi
    require_regular_nonempty_staging_file "${MARKER_FILENAME}"

    MARKER_PATH="${STAGING_DIRECTORY}/${MARKER_FILENAME}"
    MARKER_VALUE=$(/bin/cat "${MARKER_PATH}")
    case "${MARKER_VALUE}" in
        *'
'*) fail "Marker contains more than one line: ${MARKER_FILENAME}" ;;
    esac
    if [ "${MARKER_VALUE}" != "${RUN_ID}" ]; then
        fail "Marker does not match .run-id: ${MARKER_FILENAME}"
    fi

    MARKER_BYTE_COUNT=$(LC_ALL=C /usr/bin/wc -c < "${MARKER_PATH}" | /usr/bin/tr -d '[:space:]')
    MARKER_EXPECTED_BYTE_COUNT=${#RUN_ID}
    MARKER_EXPECTED_BYTE_COUNT_WITH_LF=$((MARKER_EXPECTED_BYTE_COUNT + 1))
    if [ "${MARKER_BYTE_COUNT}" -ne "${MARKER_EXPECTED_BYTE_COUNT}" ] \
        && [ "${MARKER_BYTE_COUNT}" -ne "${MARKER_EXPECTED_BYTE_COUNT_WITH_LF}" ]; then
        fail "Marker contains data beyond the run ID: ${MARKER_FILENAME}"
    fi
}

reject_white_svgs_in_directory()
{
    WHITE_SVG_DIRECTORY=$1

    for WHITE_SVG_PATH in "${WHITE_SVG_DIRECTORY}"/*-white.svg "${WHITE_SVG_DIRECTORY}"/.*-white.svg; do
        if [ ! -e "${WHITE_SVG_PATH}" ] && [ ! -L "${WHITE_SVG_PATH}" ]; then
            continue
        fi
        fail "Unexpected white SVG exists: ${WHITE_SVG_PATH##*/}"
    done
}

remove_allowlisted_final_file()
{
    FINAL_FILENAME=$1

    if ! ui_screenshot_list_contains "${FINAL_FILENAME}" "${UI_SCREENSHOT_FINAL_CLEANUP_FILES}"; then
        fail "Refusing to remove a filename outside the final-output allowlist: ${FINAL_FILENAME}"
    fi
    case "${FINAL_FILENAME}" in
        ''|*/*|.|..) fail "Invalid final-output cleanup filename: ${FINAL_FILENAME}" ;;
    esac

    FINAL_TARGET="${FINAL_DIRECTORY}/${FINAL_FILENAME}"
    if [ -L "${FINAL_TARGET}" ]; then
        fail "Refusing to replace a symbolic link in the final directory: ${FINAL_FILENAME}"
    fi
    if [ ! -e "${FINAL_TARGET}" ]; then
        return
    fi
    if [ ! -f "${FINAL_TARGET}" ]; then
        fail "Refusing to remove a non-file final-output target: ${FINAL_FILENAME}"
    fi

    printf 'Removing stale final-output file: %s\n' "${FINAL_FILENAME}"
    /bin/rm -f "${FINAL_TARGET}"
}

cleanup_export_temp()
{
    if [ -L "${EXPORT_TEMP_PATH}" ]; then
        printf 'Nextcloud UI Screenshots: refusing to remove symbolic-link export temporary file: %s\n' "${EXPORT_TEMP_PATH}" >&2
        return
    fi
    if [ -e "${EXPORT_TEMP_PATH}" ] && [ -f "${EXPORT_TEMP_PATH}" ]; then
        /bin/rm -f "${EXPORT_TEMP_PATH}"
    fi
}

copy_deliverable_atomically()
{
    COPY_FILENAME=$1

    if ! ui_screenshot_list_contains "${COPY_FILENAME}" "${UI_SCREENSHOT_DELIVERABLE_FILES}"; then
        fail "Refusing to copy a filename outside the deliverable allowlist: ${COPY_FILENAME}"
    fi
    case "${COPY_FILENAME}" in
        ''|*/*|.|..) fail "Invalid deliverable filename: ${COPY_FILENAME}" ;;
    esac

    COPY_SOURCE="${STAGING_DIRECTORY}/${COPY_FILENAME}"
    COPY_DESTINATION="${FINAL_DIRECTORY}/${COPY_FILENAME}"
    require_regular_nonempty_staging_file "${COPY_FILENAME}"

    if [ -e "${EXPORT_TEMP_PATH}" ] || [ -L "${EXPORT_TEMP_PATH}" ]; then
        fail "Export temporary path unexpectedly exists: ${UI_SCREENSHOT_EXPORT_TEMP_FILE}"
    fi
    if [ -e "${COPY_DESTINATION}" ] || [ -L "${COPY_DESTINATION}" ]; then
        fail "Final destination unexpectedly exists before atomic copy: ${COPY_FILENAME}"
    fi

    /bin/cp "${COPY_SOURCE}" "${EXPORT_TEMP_PATH}"
    if [ -L "${EXPORT_TEMP_PATH}" ] || [ ! -f "${EXPORT_TEMP_PATH}" ] || [ ! -s "${EXPORT_TEMP_PATH}" ]; then
        fail "Atomic copy did not create a regular nonempty temporary file for: ${COPY_FILENAME}"
    fi
    /bin/mv "${EXPORT_TEMP_PATH}" "${COPY_DESTINATION}"

    if [ -L "${COPY_DESTINATION}" ] || [ ! -f "${COPY_DESTINATION}" ] || [ ! -s "${COPY_DESTINATION}" ]; then
        fail "Atomic copy did not create a regular nonempty final file: ${COPY_FILENAME}"
    fi
}

validate_home_directory

APP_CONTAINER_DIRECTORY="${HOME_DIRECTORY}/Library/Containers/com.nextcloud.desktopclient"
APP_CONTAINER_DATA_DIRECTORY="${APP_CONTAINER_DIRECTORY}/Data"
APP_CONTAINER_TMP_DIRECTORY="${APP_CONTAINER_DATA_DIRECTORY}/tmp"
STAGING_DIRECTORY="${APP_CONTAINER_TMP_DIRECTORY}/nextcloud-ui-screenshots"

require_fixed_directory "${HOME_DIRECTORY}/Library"
require_fixed_directory "${HOME_DIRECTORY}/Library/Containers"
require_fixed_directory "${APP_CONTAINER_DIRECTORY}"
require_fixed_directory "${APP_CONTAINER_DATA_DIRECTORY}"
require_fixed_directory "${APP_CONTAINER_TMP_DIRECTORY}"
require_fixed_directory "${STAGING_DIRECTORY}"

CANONICAL_TMP_DIRECTORY=$(CDPATH= cd "${APP_CONTAINER_TMP_DIRECTORY}" && /bin/pwd -P)
CANONICAL_STAGING_DIRECTORY=$(CDPATH= cd "${STAGING_DIRECTORY}" && /bin/pwd -P)
if [ "${CANONICAL_TMP_DIRECTORY}" != "${APP_CONTAINER_TMP_DIRECTORY}" ]; then
    fail "The app-container temporary directory resolves unexpectedly: ${APP_CONTAINER_TMP_DIRECTORY}"
fi
if [ "${CANONICAL_STAGING_DIRECTORY}" != "${APP_CONTAINER_TMP_DIRECTORY}/nextcloud-ui-screenshots" ]; then
    fail "The staging directory is outside the expected app-container temporary directory: ${STAGING_DIRECTORY}"
fi

read_run_id
require_matching_marker '.qml-complete'

if [ -e "${STAGING_DIRECTORY}/.native-complete" ] || [ -L "${STAGING_DIRECTORY}/.native-complete" ]; then
    fail '.native-complete exists before native capture starts.'
fi

for QML_FILENAME in ${UI_SCREENSHOT_QML_OWNED_FILES}; do
    require_regular_nonempty_staging_file "${QML_FILENAME}"
done
reject_white_svgs_in_directory "${STAGING_DIRECTORY}"

if [ -L "${UI_SCREENSHOT_EXECUTABLE}" ]; then
    fail "Screenshot executable is a symbolic link: ${UI_SCREENSHOT_EXECUTABLE}"
fi
if [ ! -f "${UI_SCREENSHOT_EXECUTABLE}" ] || [ ! -x "${UI_SCREENSHOT_EXECUTABLE}" ]; then
    fail "Screenshot executable is missing or not executable: ${UI_SCREENSHOT_EXECUTABLE}"
fi

printf 'Starting native screenshot phase for run %s.\n' "${RUN_ID}"
set +e
NEXTCLOUD_UI_SCREENSHOTS=native \
NEXTCLOUD_UI_SCREENSHOT_OUTPUT="${STAGING_DIRECTORY}" \
NEXTCLOUD_UI_SCREENSHOT_RUN_ID="${RUN_ID}" \
    "${UI_SCREENSHOT_EXECUTABLE}"
NATIVE_STATUS=$?
set -e
if [ "${NATIVE_STATUS}" -ne 0 ]; then
    printf 'Nextcloud UI Screenshots: native phase failed with exit status %s.\n' "${NATIVE_STATUS}" >&2
    exit "${NATIVE_STATUS}"
fi

/bin/sh "${SCRIPT_DIRECTORY}/verify-output.sh" "${STAGING_DIRECTORY}" "${RUN_ID}"

DOWNLOADS_DIRECTORY="${HOME_DIRECTORY}/Downloads"
if [ -L "${DOWNLOADS_DIRECTORY}" ]; then
    fail "Downloads is a symbolic link: ${DOWNLOADS_DIRECTORY}"
fi
if [ ! -d "${DOWNLOADS_DIRECTORY}" ]; then
    fail "The real user's Downloads directory does not exist: ${DOWNLOADS_DIRECTORY}"
fi
CANONICAL_DOWNLOADS_DIRECTORY=$(CDPATH= cd "${DOWNLOADS_DIRECTORY}" && /bin/pwd -P)
if [ "${CANONICAL_DOWNLOADS_DIRECTORY}" != "${HOME_DIRECTORY}/Downloads" ]; then
    fail "Downloads does not resolve to the real user's Downloads directory: ${DOWNLOADS_DIRECTORY}"
fi

FINAL_DIRECTORY="${DOWNLOADS_DIRECTORY}/Nextcloud UI Screenshots"
if [ -L "${FINAL_DIRECTORY}" ]; then
    fail "Final output directory is a symbolic link: ${FINAL_DIRECTORY}"
fi
if [ -e "${FINAL_DIRECTORY}" ]; then
    if [ ! -d "${FINAL_DIRECTORY}" ]; then
        fail "Final output path is not a directory: ${FINAL_DIRECTORY}"
    fi
else
    /bin/mkdir "${FINAL_DIRECTORY}" || fail "Could not create final output directory: ${FINAL_DIRECTORY}"
fi
if [ -L "${FINAL_DIRECTORY}" ] || [ ! -d "${FINAL_DIRECTORY}" ]; then
    fail "Final output path is not a safe directory: ${FINAL_DIRECTORY}"
fi
CANONICAL_FINAL_DIRECTORY=$(CDPATH= cd "${FINAL_DIRECTORY}" && /bin/pwd -P)
if [ "${CANONICAL_FINAL_DIRECTORY}" != "${DOWNLOADS_DIRECTORY}/Nextcloud UI Screenshots" ]; then
    fail "Final output directory resolves unexpectedly: ${FINAL_DIRECTORY}"
fi

EXPORT_TEMP_PATH="${FINAL_DIRECTORY}/${UI_SCREENSHOT_EXPORT_TEMP_FILE}"
trap cleanup_export_temp 0

for FINAL_CLEANUP_NAME in ${UI_SCREENSHOT_FINAL_CLEANUP_FILES}; do
    remove_allowlisted_final_file "${FINAL_CLEANUP_NAME}"
done
reject_white_svgs_in_directory "${FINAL_DIRECTORY}"

for DELIVERABLE_FILENAME in ${UI_SCREENSHOT_DELIVERABLE_FILES}; do
    copy_deliverable_atomically "${DELIVERABLE_FILENAME}"
done

/bin/sh "${SCRIPT_DIRECTORY}/verify-output.sh" --deliverables-only "${FINAL_DIRECTORY}"

# Keep this as the final success line so Xcode exposes the destination clearly.
printf '%s\n' "${FINAL_DIRECTORY}"
