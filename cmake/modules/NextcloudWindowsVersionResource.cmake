# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Embeds the client's file version into a Windows DLL target, only when WIN_MSI_LEGACY_UPGRADE_CODE is set.
#
# Windows Installer never replaces an installed file that has a version with a file that has none,
# so a DLL without a version resource would not overwrite the one of a legacy product.
function(nextcloud_add_windows_version_resource target)
    if(NOT WIN32)
        return()
    endif()

    set(NC_VERSION_FILETYPE VFT_DLL)
    set(version_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}_version.rc")
    configure_file("${CMAKE_SOURCE_DIR}/src/gui/version.rc.in" "${version_rc}" @ONLY)
    target_sources(${target} PRIVATE "${version_rc}")
endfunction()
