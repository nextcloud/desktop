/*
 * SPDX-FileCopyrightText: 2022 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QtCore>
#include <QQuickImageProvider>

namespace OCC {

/**
 * @brief The TrayImageProvider
 * @ingroup gui
 * Allows to fetch icon from the server or used a local resource
 */

class TrayImageProvider : public QQuickAsyncImageProvider
{
public:
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;
};
}
