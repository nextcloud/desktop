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

#include <QClipboard>
#include <QApplication>
#include <QDesktopServices>
#include <QLoggingCategory>
#include <QMessageBox>

using namespace OCC;

Q_LOGGING_CATEGORY(lcUtility, "gui.utility", QtInfoMsg)

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
        qCWarning(lcUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}

bool Utility::openEmailComposer(const QString &subject, const QString &body, QWidget *errorWidgetParent)
{
    QUrl url(QLatin1String("mailto:"));
    url.setQueryItems({ { QLatin1String("subject"), subject },
        { QLatin1String("body"), body } });

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
        qCWarning(lcUtility) << "QDesktopServices::openUrl failed for" << url;
        return false;
    }
    return true;
}
