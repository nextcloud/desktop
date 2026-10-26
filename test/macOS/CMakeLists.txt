# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

# Pins the identifiers the FinderSync XPC rendezvous depends on. Reads no bundle and starts
# no GUI, so it runs in PR CI — unlike the signed-bundle checks, which need a signing identity
# and a login session and therefore belong to the release pipeline.
nextcloud_add_test(FinderSyncBrokerIdentity)
nextcloud_add_test(FinderSyncPathNormalization)
nextcloud_add_test(SystraySyncControlMacOS)
nextcloud_add_test(MacSandboxUtility)

nextcloud_add_test(CocoaInitializerMacOS)
nextcloud_add_test(FinderSyncServiceBranding)
nextcloud_add_test(LocalNetworkPermissionMacOS)
target_sources(LocalNetworkPermissionMacOSTest PRIVATE localnetworkpermissiontestconnection.mm)

if(SPARKLE_FOUND AND BUILD_UPDATER)
    nextcloud_add_test(SparkleUpdaterMacOS)
    target_sources(SparkleUpdaterMacOSTest PRIVATE sparkleupdatertestcontroller.mm)
    target_link_libraries(SparkleUpdaterMacOSTest PRIVATE ${SPARKLE_LIBRARY})
endif()

set_source_files_properties(
    testcocoainitializermacos.cpp
    testfindersyncservicebranding.cpp
    testlocalnetworkpermissionmacos.cpp
    testsparkleupdatermacos.cpp
    testxpcservicelifecycle.cpp
    PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
set_property(SOURCE
    localnetworkpermissiontestconnection.mm
    sparkleupdatertestcontroller.mm
    xpcservicetestpeer.mm
    APPEND PROPERTY COMPILE_OPTIONS -fobjc-arc)

if(BUILD_FILE_PROVIDER_MODULE)
    nextcloud_add_test(XPCServiceLifecycle)
    target_sources(XPCServiceLifecycleTest PRIVATE xpcservicetestpeer.mm)
    nextcloud_add_test(FileProviderXPCUtils)
    set_source_files_properties(testfileproviderxpcutils.cpp PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
endif()
