# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nextcloudGuiSharing STATIC)
ecm_add_qml_module(nextcloudGuiSharing
  URI com.nextcloud.desktopclient.sharing
  GENERATE_PLUGIN_SOURCE
  QML_FILES
    ShareDialog.qml
)

target_link_libraries(nextcloudGuiSharing PRIVATE nextcloudCore)
