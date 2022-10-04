/*
 * Copyright (C) by Oleksandr Zolotov <alex@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once
#include "config.h"
#include <QByteArray>
#include <QString>
#include <QVariantMap>

namespace VfsShellExtensions {
QString serverNameForApplicationName(const QString &applicationName);
QString serverNameForApplicationNameDefault();

namespace Protocol {
    static constexpr auto CustomStateProviderRequestKey = "customStateProviderRequest";
    static constexpr auto CustomStateDataKey = "customStateData";
    static constexpr auto CustomStateStatesKey = "states";
    static constexpr auto FilePathKey = "filePath";
    static constexpr auto ThumbnailProviderRequestKey = "thumbnailProviderRequest";
    static constexpr auto ThumbnailProviderRequestFileSizeKey = "fileSize";
    static constexpr auto ThumnailProviderDataKey = "thumbnailData";
    static constexpr auto Version = "2.0";

    QByteArray createJsonMessage(const QVariantMap &message);
    bool validateProtocolVersion(const QVariantMap &message);
}
}
