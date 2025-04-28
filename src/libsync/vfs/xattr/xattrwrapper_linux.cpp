/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
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
