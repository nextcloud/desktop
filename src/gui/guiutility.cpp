/*
 * Copyright (C) by Christian Kamm <mail@ckamm.de>
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

#include "guiutility.h"
#include "gui/application.h"
#include "gui/settingsdialog.h"
#include "libsync/filesystem.h"
#include "libsync/theme.h"

#include <QApplication>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QNetworkInformation>
#include <QQuickWidget>
#include <QUrlQuery>

namespace OCC {
Q_LOGGING_CATEGORY(lcGuiUtility, "gui.utility", QtInfoMsg)
}

namespace {
const QString dirTag()
{
    return QStringLiteral("com.owncloud.spaces.app");
}

const QString uuidTag()
{
    return QStringLiteral("com.owncloud.spaces.account-uuid");
}
} // anonymous namespace

using namespace OCC;

bool Utility::openBrowser(const QUrl &url, QWidget *errorWidgetParent)
{
    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(
                errorWidgetParent,
                QCoreApplication::translate("utility", "Could not open browser"),
                QCoreApplication::translate("utility",
                    "There was an error when launching the browser to go to "
                    "URL %1. Maybe no default browser is configured?")
                    .arg(url.toString()));
        }
        qCWarning(lcGuiUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

bool Utility::openEmailComposer(const QString &subject, const QString &body, QWidget *errorWidgetParent)
{
    QUrl url(QStringLiteral("mailto:"));
    QUrlQuery query;
    query.setQueryItems({ { QLatin1String("subject"), subject },
        { QLatin1String("body"), body } });
    url.setQuery(query);

    if (!QDesktopServices::openUrl(url)) {
        if (errorWidgetParent) {
            QMessageBox::warning(
                errorWidgetParent,
                QCoreApplication::translate("utility", "Could not open email client"),
                QCoreApplication::translate("utility",
                    "There was an error when launching the email client to "
                    "create a new message. Maybe no default email client is "
                    "configured?"));
        }
        qCWarning(lcGuiUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

QString Utility::vfsPinActionText()
{
    return QCoreApplication::translate("utility", "Make always available locally");
}

QString Utility::vfsFreeSpaceActionText()
{
    return QCoreApplication::translate("utility", "Free up local space");
}

bool Utility::internetConnectionIsMetered()
{
    if (auto *qNetInfo = QNetworkInformation::instance()) {
        return qNetInfo->isMetered();
    }

    return false;
}

void Utility::markDirectoryAsSyncRoot(const QString &path, const QUuid &accountUuid)
{
    Q_ASSERT(getDirectorySyncRootMarkings(path).first.isEmpty());
    Q_ASSERT(getDirectorySyncRootMarkings(path).second.isNull());

    auto result1 = FileSystem::Tags::set(path, dirTag(), Theme::instance()->orgDomainName().toUtf8());
    if (!result1) {
        qCWarning(lcGuiUtility) << QStringLiteral("Failed to set tag on '%1': %2").arg(path, result1.error())
#ifdef Q_OS_WIN
                                << QStringLiteral("(filesystem %1)").arg(FileSystem::fileSystemForPath(path))
#endif // Q_OS_WIN
            ;
        return;
    }

    auto result2 = FileSystem::Tags::set(path, uuidTag(), accountUuid.toString().toUtf8());
    if (!result2) {
        qCWarning(lcGuiUtility) << QStringLiteral("Failed to set tag on '%1': %2").arg(path, result2.error())
#ifdef Q_OS_WIN
                                << QStringLiteral("(filesystem %1)").arg(FileSystem::fileSystemForPath(path))
#endif // Q_OS_WIN
            ;
        return;
    }
}

std::pair<QString, QUuid> Utility::getDirectorySyncRootMarkings(const QString &path)
{
    auto existingDirTag = FileSystem::Tags::get(path, dirTag());
    auto existingUuidTag = FileSystem::Tags::get(path, uuidTag());

    if (existingDirTag.has_value() && existingUuidTag.has_value()) {
        return {QString::fromUtf8(existingDirTag.value()), QUuid::fromString(QString::fromUtf8(existingUuidTag.value()))};
    }

    return {};
}

void Utility::unmarkDirectoryAsSyncRoot(const QString &path)
{
    if (!FileSystem::Tags::remove(path, dirTag())) {
        qCWarning(lcGuiUtility) << "Failed to remove tag on" << path;
    }
    if (!FileSystem::Tags::remove(path, uuidTag())) {
        qCWarning(lcGuiUtility) << "Failed to remove uuid tag on" << path;
    }
}
