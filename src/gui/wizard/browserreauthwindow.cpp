/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "browserreauthwindow.h"

#include "application.h"
#include "systray.h"
#include "theme.h"
#include "wizard/browserreauthcontroller.h"

#ifdef Q_OS_MACOS
#include "foregroundbackground_interface.h"
#include "nativetitlebar_mac.h"
#endif

#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QTimer>
#include <QVariant>

namespace OCC {

Q_LOGGING_CATEGORY(lcBrowserReAuthWindow, "nextcloud.gui.browserreauthwindow", QtInfoMsg)

BrowserReAuthWindow::BrowserReAuthWindow(Account *account, QObject *parent)
    : QObject(parent)
    , _controller(new BrowserReAuthController(account, this))
{
    connect(_controller, &BrowserReAuthController::credentialsReady, this, &BrowserReAuthWindow::credentialsReady);
    connect(_controller, &BrowserReAuthController::cancelled, this, &BrowserReAuthWindow::cancelled);
    connect(this, &BrowserReAuthWindow::credentialsReady, this, &BrowserReAuthWindow::close, Qt::QueuedConnection);
    connect(this, &BrowserReAuthWindow::cancelled, this, &BrowserReAuthWindow::close, Qt::QueuedConnection);

    auto *const systray = Systray::instance();
    if (!systray) {
        qCWarning(lcBrowserReAuthWindow) << "Cannot start browser re-authentication without the system tray.";
        _loadFailed = true;
        return;
    }

    auto *engine = systray->trayEngine();
    if (!engine) {
        qCWarning(lcBrowserReAuthWindow) << "Cannot start browser re-authentication without a QML engine.";
        _loadFailed = true;
        return;
    }

    QQmlComponent component(engine, QStringLiteral("qrc:/qml/src/gui/wizard/qml/BrowserReAuthWindow.qml"));
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("controller"), QVariant::fromValue<QObject *>(_controller));
    auto *createdObject = component.createWithInitialProperties(initialProperties);

    if (component.isError()) {
        qCWarning(lcBrowserReAuthWindow) << "Failed to load QML browser re-authentication window:" << component.errors();
    }

    _window = qobject_cast<QQuickWindow *>(createdObject);
    if (!_window) {
        if (createdObject) {
            createdObject->deleteLater();
        }
        _loadFailed = true;
        return;
    }

    createdObject->setParent(this);
    _window->setIcon(Theme::instance()->applicationIcon());

#ifdef Q_OS_MACOS
    auto *fgbg = new ForegroundBackground(this);
    _window->installEventFilter(fgbg);
    styleNativeTitleBar(_window, /*hideTitleText=*/true);
    connect(_window, &QQuickWindow::colorChanged, this, [this] {
        if (_window) {
            styleNativeTitleBar(_window, /*hideTitleText=*/true);
        }
    });
#endif

    connect(_window, &QQuickWindow::activeChanged, this, [this] {
        if (_window && _window->isActive()) {
            _controller->pollNow();
        }
    });

    if (auto *app = dynamic_cast<Application *>(qApp)) {
        connect(app, &Application::isShowingSettingsDialog, this, &BrowserReAuthWindow::slotShowSettingsDialog);
    }
}

BrowserReAuthWindow::~BrowserReAuthWindow()
{
    if (_window) {
        delete _window.data();
    }
}

void BrowserReAuthWindow::setInfoText(const QString &infoText)
{
    _controller->setInfoText(infoText);
}

void BrowserReAuthWindow::show()
{
    if (_loadFailed || !_window) {
        QTimer::singleShot(0, this, [this] {
            emit cancelled();
        });
        return;
    }

    _window->show();
    _window->raise();
    _window->requestActivate();
    _controller->start();
}

void BrowserReAuthWindow::close()
{
    if (_window) {
        _window->close();
    }
    deleteLater();
}

void BrowserReAuthWindow::slotShowSettingsDialog()
{
    QTimer::singleShot(100, this, [this] {
        if (!_window) {
            return;
        }

        _window->show();
        _window->raise();
        _window->requestActivate();
    });
}

} // namespace OCC
