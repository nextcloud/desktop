# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nextcloudGuiSharing STATIC)

set_source_files_properties(RecipientIcon.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

target_sources(nextcloudGuiSharing
  PRIVATE
    sharedetailslistmodel.h
    sharedetailslistmodel.cpp
    sharingconstants.h
    unifiedsharingrequest.h
    unifiedsharingrequest.cpp
    createsharejob.h
    createsharejob.cpp
    destroysharejob.h
    destroysharejob.cpp
    generatesecretjob.h
    generatesecretjob.cpp
    getsharejob.h
    getsharejob.cpp
    getsharesjob.h
    getsharesjob.cpp
    searchrecipientsjob.h
    searchrecipientsjob.cpp
    updatesharejob.h
    updatesharejob.cpp
    addsourcejob.h
    addsourcejob.cpp
    removesourcejob.h
    removesourcejob.cpp
    addrecipientjob.h
    addrecipientjob.cpp
    removerecipientjob.h
    removerecipientjob.cpp
    setpropertyjob.h
    setpropertyjob.cpp
    setpermissionjob.h
    setpermissionjob.cpp
    setpermissionpresetjob.h
    setpermissionpresetjob.cpp
    setrecipientsecretjob.h
    setrecipientsecretjob.cpp
    setsharestatejob.h
    setsharestatejob.cpp
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
    recipienticonutils.h
    recipienticonutils.cpp
    recipientmodel.h
    recipientmodel.cpp
    recipientsearchmodel.h
    recipientsearchmodel.cpp
    unifiedshare.h
    unifiedshare.cpp
    sharingcontroller.h
    sharingcontroller.cpp
    unifiedsharelistmodel.h
    unifiedsharelistmodel.cpp
)

ecm_add_qml_module(nextcloudGuiSharing
  URI com.nextcloud.desktopclient.sharing
  GENERATE_PLUGIN_SOURCE
  QML_FILES
    FieldDelegate.qml
    RecipientIcon.qml
    RecipientSearchField.qml
    ShareActionRow.qml
    ShareDetailsPage.qml
    ShareListPage.qml
    ShareRow.qml
    ShareSectionHeader.qml
    ShareDialog.qml
)

target_link_libraries(nextcloudGuiSharing PRIVATE nextcloudCore)
