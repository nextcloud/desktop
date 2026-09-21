/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#include "accountfwd.h"
#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <functional>
#include <memory>

class QQuickWindow;
class QWidget;

namespace OCC::DocAssets
{
class CaptureScene : public QObject
{
public:
    CaptureScene();
    ~CaptureScene() override;
    QUrl source;
    QString module;
    QString type;
    QVariantMap properties;
    AccountPtr account;
    AccountStatePtr accountState;
    std::unique_ptr<QWidget> widget;
    bool expectsPopup = false;
    std::function<bool(QString *)> ready = [](QString *) {
        return true;
    };
    std::function<bool(QQuickWindow *, QString *)> settled = [](QQuickWindow *, QString *) {
        return true;
    };
    std::function<bool(QQuickWindow *, QString *)> activate = [](QQuickWindow *, QString *) {
        return true;
    };
};
}
