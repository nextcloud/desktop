# SPDX-FileCopyrightText: 2025 Nextcloud GmbH
# SPDX-FileContributor: Carl Schwan <carl.schwan@nextcloud.com>
# SPDX-License-Identifier: BSD-3-Clause

add_library(nextcloudTray STATIC
    activitydata.cpp
    activitydata.h
    activitylistmodel.cpp
    activitylistmodel.h
    asyncimageresponse.cpp
    fileactivitylistmodel.cpp
    fileactivitylistmodel.h
    notificationhandler.cpp
    notificationhandler.h
    sortedactivitylistmodel.cpp
    sortedactivitylistmodel.h
    svgimageprovider.cpp
    svgimageprovider.h
    syncstatussummary.cpp
    syncstatussummary.h
    talkreply.cpp
    trayimageprovider.cpp
    unifiedsearchresult.cpp
    unifiedsearchresult.h
    unifiedsearchresultslistmodel.cpp
    unifiedsearchresultslistmodel.h
    usermodel.cpp
    usermodel.h
)

target_link_libraries(nextcloudTray PRIVATE nextcloudCore)

qt_add_qml_module(nextcloudTray
    URI com.nextcloud.desktopclient.tray
    QML_FILES 
        MainWindow.qml
        UserLine.qml
        HeaderButton.qml
        SyncStatus.qml
        ActivityItem.qml
        AutoSizingMenu.qml
        ActivityList.qml
        CurrentAccountHeaderButton.qml
        TrayWindowHeader.qml
        UnifiedSearchInputContainer.qml
        UnifiedSearchResultFetchMoreTrigger.qml
        UnifiedSearchResultItem.qml
        UnifiedSearchResultItemSkeleton.qml
        UnifiedSearchResultItemSkeletonContainer.qml
        UnifiedSearchResultItemSkeletonGradientRectangle.qml
        UnifiedSearchResultListItem.qml
        UnifiedSearchResultNothingFound.qml
        UnifiedSearchPlaceholderView.qml
        UnifiedSearchResultSectionItem.qml
        ActivityItemContextMenu.qml
        ActivityItemActions.qml
        ActivityItemContent.qml
        TalkReplyTextField.qml
        CallNotificationDialog.qml
        EditFileLocallyLoadingDialog.qml
        EncryptionTokenDiscoveryDialog.qml
        NCBusyIndicator.qml
        NCIconWithBackgroundImage.qml
        NCProgressBar.qml
        EnforcedPlainTextLabel.qml
        ListItemLineAndSubline.qml
        TrayFoldersMenuButton.qml
        TrayFolderListItem.qml
)
