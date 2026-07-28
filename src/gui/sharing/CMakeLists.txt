# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nextcloudGuiSharing STATIC)

target_sources(nextcloudGuiSharing
  PRIVATE
    abstractsharemodel.h
    abstractsharemodel.cpp
    unifiedsharingrequest.h
    unifiedsharingrequest.cpp
    createsharejob.h
    createsharejob.cpp
    destroysharejob.h
    destroysharejob.cpp
    searchrecipientsjob.h
    searchrecipientsjob.cpp
    updatesharejob.h
    updatesharejob.cpp
    addsourcejob.h
    addsourcejob.cpp
    addrecipientjob.h
    addrecipientjob.cpp
    removerecipientjob.h
    removerecipientjob.cpp
    setpermissionjob.h
    setpermissionjob.cpp
    setpermissionpresetjob.h
    setpermissionpresetjob.cpp
    permission.h
    permission.cpp
    permissionmodel.h
    permissionmodel.cpp
    property.h
    property.cpp
    propertymodel.h
    propertymodel.cpp
    recipient.h
    recipient.cpp
    recipientmodel.h
    recipientmodel.cpp
    recipientsearchmodel.h
    recipientsearchmodel.cpp
    share.h
    share.cpp
    sharingcontroller.h
    sharingcontroller.cpp
)

ecm_add_qml_module(nextcloudGuiSharing
  URI com.nextcloud.desktopclient.sharing
  GENERATE_PLUGIN_SOURCE
  QML_FILES
    FieldDelegate.qml
    MainPage.qml
    RecipientSearchField.qml
    SettingsPage.qml
    ShareDialog.qml
)

target_link_libraries(nextcloudGuiSharing PRIVATE nextcloudCore)
