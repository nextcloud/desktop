# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Pins the identifiers the FinderSync XPC rendezvous depends on. Reads no bundle and starts
# no GUI, so it runs in PR CI — unlike the signed-bundle checks, which need a signing identity
# and a login session and therefore belong to the release pipeline.
nextcloud_add_test(FinderSyncBrokerIdentity)
nextcloud_add_test(FinderSyncPathNormalization)
nextcloud_add_test(SystraySyncControlMacOS)
nextcloud_add_test(MacSandboxUtility)

if(BUILD_FILE_PROVIDER_MODULE)
    nextcloud_add_test(FileProviderXPCUtils)
    target_sources(FileProviderXPCUtilsTest PRIVATE testfileprovideritem.mm)
    # Keep NSUserDefaults writes out of the application's preferences domain.
    set_target_properties(FileProviderXPCUtilsTest PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_ROOT}/Modules/MacOSXBundleInfo.plist.in"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.nextcloud.desktopclient.tests.fileproviderxpcutils")
    set_source_files_properties(testfileproviderxpcutils.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
    set_property(SOURCE testfileprovideritem.mm APPEND PROPERTY COMPILE_OPTIONS -fobjc-arc)
endif()
