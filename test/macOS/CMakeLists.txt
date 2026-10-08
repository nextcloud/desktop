# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Pins the identifiers the FinderSync XPC rendezvous depends on. Reads no bundle and starts
# no GUI, so it runs in PR CI — unlike the signed-bundle checks, which need a signing identity
# and a login session and therefore belong to the release pipeline.
nextcloud_add_test(FinderSyncBrokerIdentity)
nextcloud_add_test(FinderSyncPathNormalization)
nextcloud_add_test(SystraySyncControlMacOS)
nextcloud_add_test(MacSandboxUtility)

set(native_tests
    MacUtility
    MacAppIcon
    MacSandboxSecurityScopedAccess
    MacSandboxPersistentAccess
    MacTrayPopupImageUtils
    MacTrayPopupViewUtils
    MacTrayHoverView
    MacTrayAccountRow
    MacTrayRows
)
if(BUILD_FILE_PROVIDER_MODULE)
    list(APPEND native_tests MacProgressObserver)
endif()

set(native_test_sources
    macutilitymock.mm
    securityscopedaccessmock.mm
    ncaccountrowdelegatemock.mm
    traypopupviewtestutils.mm
)
foreach(native_test IN LISTS native_tests)
    nextcloud_add_test(${native_test})
    string(TOLOWER "${native_test}" native_test_lowercase)
    set(native_test_source "test${native_test_lowercase}.cpp")
    set_source_files_properties(${native_test_source} PROPERTIES COMPILE_FLAGS "-x objective-c++")
    list(APPEND native_test_sources ${native_test_source})
endforeach()
set_property(SOURCE ${native_test_sources} APPEND PROPERTY COMPILE_OPTIONS -fobjc-arc)

target_sources(MacUtilityTest PRIVATE macutilitymock.h macutilitymock.mm)
foreach(native_test IN ITEMS MacSandboxSecurityScopedAccess MacSandboxPersistentAccess)
    target_sources(${native_test}Test PRIVATE securityscopedaccessmock.h securityscopedaccessmock.mm)
endforeach()
foreach(native_test IN ITEMS MacTrayPopupViewUtils MacTrayAccountRow MacTrayRows)
    target_sources(${native_test}Test PRIVATE traypopupviewtestutils.h traypopupviewtestutils.mm)
endforeach()
target_sources(MacTrayAccountRowTest PRIVATE ncaccountrowdelegatemock.h ncaccountrowdelegatemock.mm)

if(BUILD_FILE_PROVIDER_MODULE)
    nextcloud_add_test(FileProviderXPCUtils)
    set_source_files_properties(testfileproviderxpcutils.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++")
endif()
