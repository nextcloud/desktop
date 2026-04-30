# SPDX-FileCopyrightText: 2025 Nextcloud GmbH
# SPDX-FileContributor: Carl Schwan <carl.schwan@nextcloud.com>
# SPDX-License-Identifier: BSD-3-Clause

add_library(nextcloudMacOS STATIC)

qt_add_qml_module(nextcloudMacOS
    URI com.nextcloud.desktopclient.nextcloud.macos
    QML_FILES 
	ui/FileProviderSettings.qml
	ui/FileProviderFileDelegate.qml
)
