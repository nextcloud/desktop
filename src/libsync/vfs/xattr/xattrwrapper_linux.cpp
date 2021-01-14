/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include "xattrwrapper.h"

#include "config.h"

#include <QLoggingCategory>

#include <sys/xattr.h>

Q_LOGGING_CATEGORY(lcXAttrWrapper, "nextcloud.sync.vfs.xattr.wrapper", QtInfoMsg)

namespace {
constexpr auto hydrateExecAttributeName = "user.nextcloud.hydrate_exec";

OCC::Optional<QByteArray> xattrGet(const QByteArray &path, const QByteArray &name)
{
    constexpr auto bufferSize = 256;
    QByteArray result;
    result.resize(bufferSize);
    const auto count = getxattr(path.constData(), name.constData(), result.data(), bufferSize);
    if (count >= 0) {
        result.resize(static_cast<int>(count) - 1);
        return result;
    } else {
        return {};
    }
}

bool xattrSet(const QByteArray &path, const QByteArray &name, const QByteArray &value)
{
    const auto returnCode = setxattr(path.constData(), name.constData(), value.constData(), value.size() + 1, 0);
    return returnCode == 0;
}

}


bool OCC::XAttrWrapper::hasNextcloudPlaceholderAttributes(const QString &path)
{
    const auto value = xattrGet(path.toUtf8(), hydrateExecAttributeName);
    if (value) {
        return *value == QByteArrayLiteral(APPLICATION_EXECUTABLE);
    } else {
        return false;
    }
}

OCC::Result<void, QString> OCC::XAttrWrapper::addNextcloudPlaceholderAttributes(const QString &path)
{
    const auto success = xattrSet(path.toUtf8(), hydrateExecAttributeName, APPLICATION_EXECUTABLE);
    if (!success) {
        return QStringLiteral("Failed to set the extended attribute");
    } else {
        return {};
    }
}
