# SPDX-FileCopyrightText: 2025 Nextcloud GmbH
# SPDX-FileContributor: Carl Schwan <carl.schwan@nextcloud.com>
# SPDX-License-Identifier: BSD-3-Clause

add_library(nextcloudFileDetails STATIC)

qt_add_qml_module(nextcloudFileDetails
    URI com.nextcloud.desktopclient.filedetails
    QML_FILES 
        FileActivityView.qml
        FileDetailsPage.qml
        FileDetailsView.qml
        FileDetailsWindow.qml
        FileTag.qml
        NCInputDateField.qml
        NCInputTextArea.qml
        NCInputTextField.qml
        NCTabButton.qml
        ShareeDelegate.qml
        ShareDelegate.qml
        ShareDetailsPage.qml
        ShareeSearchField.qml
        ShareView.qml
    SOURCES
        datefieldbackend.h
        datefieldbackend.cpp
        filedetails.h
        filedetails.cpp
        sharemodel.h
        sharemodel.cpp
        shareemodel.h
        shareemodel.cpp
        sortedsharemodel.h
        sortedsharemodel.cpp
)

target_link_libraries(nextcloudFileDetails PRIVATE nextcloudCore)
