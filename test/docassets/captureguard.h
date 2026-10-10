/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include <QObject>
#include <QUrl>
namespace OCC::DocAssets
{
class CaptureGuard final : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;
    bool eventFilter(QObject *object, QEvent *event) override;
public Q_SLOTS:
    void consumeUrl(const QUrl &url);
};
}
