/*
 * SPDX-FileCopyrightText: 2021 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <QQuickImageProvider>

namespace OCC {
namespace Ui {
    class SvgImageProvider : public QQuickImageProvider
    {
    public:
        SvgImageProvider();
        QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
    };
}
}
