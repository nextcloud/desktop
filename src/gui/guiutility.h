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

#pragma once

#include "gui/owncloudguilib.h"

#include "common/pinstate.h"

#include <QLoggingCategory>
#include <QString>
#include <QUrl>
#include <QWidget>


class QQuickWidget;

namespace OCC {

Q_DECLARE_LOGGING_CATEGORY(lcGuiUtility)

namespace Utility {

    /** Open an url in the browser.
     *
     * If launching the browser fails, display a message.
     */
    bool openBrowser(const QUrl &url, QWidget *errorWidgetParent);

    /** Start composing a new email message.
     *
     * If launching the email program fails, display a message.
     */
    bool openEmailComposer(const QString &subject, const QString &body,
        QWidget *errorWidgetParent);

    /** Translated text for "making items always available locally" */
    QString vfsPinActionText();

    /** Translated text for "free up local space" (and unpinning the item) */
    QString vfsFreeSpaceActionText();

    void startShellIntegration();

    QString socketApiSocketPath();

    bool internetConnectionIsMetered();

    OWNCLOUDGUI_EXPORT void markDirectoryAsSyncRoot(const QString &path, const QUuid &accountUuid);
    std::pair<QString, QUuid> getDirectorySyncRootMarkings(const QString &path);
    void unmarkDirectoryAsSyncRoot(const QString &path);

    void initQuickWidget(QQuickWidget *widget, const QUrl &src);

} // namespace Utility
} // namespace OCC
