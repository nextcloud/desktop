#!/bin/sh
# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Deliverable and excluded filenames come only from the authoritative manifest.
# Callers must never turn user-controlled input or a directory listing into a deletion list.
UI_SCREENSHOT_MANIFEST_FILE="${SCRIPT_DIRECTORY}/manifest.tsv"
if [ ! -f "${UI_SCREENSHOT_MANIFEST_FILE}" ]; then
    printf 'Nextcloud UI Screenshots: manifest is missing: %s\n' "${UI_SCREENSHOT_MANIFEST_FILE}" >&2
    return 1 2>/dev/null || exit 1
fi

ui_screenshot_manifest_outputs()
{
    /usr/bin/awk -F '\t' -v category="$1" '$1 == category { print $3 }' "${UI_SCREENSHOT_MANIFEST_FILE}"
}

UI_SCREENSHOT_QML_PNG_FILES=$(ui_screenshot_manifest_outputs qml)
UI_SCREENSHOT_NATIVE_PNG_FILES=$(ui_screenshot_manifest_outputs native)

UI_SCREENSHOT_PNG_FILES="${UI_SCREENSHOT_QML_PNG_FILES}
${UI_SCREENSHOT_NATIVE_PNG_FILES}"

UI_SCREENSHOT_SVG_FILES=$(ui_screenshot_manifest_outputs svg)

UI_SCREENSHOT_QML_OWNED_FILES="${UI_SCREENSHOT_QML_PNG_FILES}
${UI_SCREENSHOT_SVG_FILES}"

UI_SCREENSHOT_NATIVE_OWNED_FILES="${UI_SCREENSHOT_NATIVE_PNG_FILES}"

UI_SCREENSHOT_DELIVERABLE_FILES="${UI_SCREENSHOT_PNG_FILES}
${UI_SCREENSHOT_SVG_FILES}"

UI_SCREENSHOT_EXCLUDED_PNG_FILES=$(ui_screenshot_manifest_outputs excluded-png)

# These are the six known white-state exports from older screenshot tooling.
# Any other filename matching *-white.svg is unknown and must cause a failure.
UI_SCREENSHOT_WHITE_SVG_FILES=$(ui_screenshot_manifest_outputs white-svg)

UI_SCREENSHOT_MARKER_FILES='.run-id
.qml-complete
.native-complete'

UI_SCREENSHOT_STAGING_CLEANUP_FILES="${UI_SCREENSHOT_DELIVERABLE_FILES}
${UI_SCREENSHOT_EXCLUDED_PNG_FILES}
${UI_SCREENSHOT_WHITE_SVG_FILES}
${UI_SCREENSHOT_MARKER_FILES}"

UI_SCREENSHOT_NATIVE_CLEANUP_FILES="${UI_SCREENSHOT_NATIVE_OWNED_FILES}
.native-complete"

UI_SCREENSHOT_EXPORT_TEMP_FILE='.nextcloud-ui-screenshots-export.tmp'

UI_SCREENSHOT_FINAL_CLEANUP_FILES="${UI_SCREENSHOT_DELIVERABLE_FILES}
${UI_SCREENSHOT_EXCLUDED_PNG_FILES}
${UI_SCREENSHOT_WHITE_SVG_FILES}
${UI_SCREENSHOT_EXPORT_TEMP_FILE}"

ui_screenshot_list_contains()
{
    ui_screenshot_needle=$1
    ui_screenshot_haystack=$2

    for ui_screenshot_listed_name in ${ui_screenshot_haystack}; do
        if [ "${ui_screenshot_listed_name}" = "${ui_screenshot_needle}" ]; then
            return 0
        fi
    done

    return 1
}
