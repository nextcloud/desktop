# SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
# SPDX-License-Identifier: GPL-2.0-or-later

add_library(nextcloudGuiSearch STATIC)

target_sources(nextcloudGuiSearch
  PRIVATE
    unifiedsearchresult.h
    unifiedsearchresult.cpp
    unifiedsearchresultslistmodel.h
    unifiedsearchresultslistmodel.cpp
)

ecm_add_qml_module(nextcloudGuiSearch
  URI com.nextcloud.desktopclient.search
  GENERATE_PLUGIN_SOURCE
  QML_FILES
    SearchWindow.qml
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
)

target_link_libraries(nextcloudGuiSearch PRIVATE nextcloudCore)
