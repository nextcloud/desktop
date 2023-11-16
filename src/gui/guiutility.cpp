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
#include "application.h"
#include "settingsdialog.h"

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QUrlQuery>
#include <QIcon>

#include "theme.h"

#include "common/asserts.h"
#include "libsync/filesystem.h"

namespace OCC {
Q_LOGGING_CATEGORY(lcGuiUtility, "gui.utility", QtInfoMsg)
}

namespace {
const QString dirTag()
{
    return QStringLiteral("com.owncloud.spaces.app");
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

void Utility::markDirectoryAsSyncRoot(const QString &path)
{
    Q_ASSERT(getDirectorySyncRootMarking(path).isEmpty());

    auto result = FileSystem::Tags::set(path, dirTag(), Theme::instance()->orgDomainName().toUtf8());
    if (!result) {
        qCWarning(lcGuiUtility) << QStringLiteral("Failed to set tag on '%1': %2").arg(path, result.error())
#ifdef Q_OS_WIN
                                << QStringLiteral("(filesystem %1)").arg(FileSystem::fileSystemForPath(path))
#endif // Q_OS_WIN
            ;
    }
}

QString Utility::getDirectorySyncRootMarking(const QString &path)
{
    auto existingValue = FileSystem::Tags::get(path, dirTag());
    if (existingValue.has_value()) {
        return QString::fromUtf8(existingValue.value());
    }

    return {};
}

void Utility::unmarkDirectoryAsSyncRoot(const QString &path)
{
    if (!FileSystem::Tags::remove(path, dirTag())) {
        qCWarning(lcGuiUtility) << "Failed to remove tag on" << path;
    }
}
