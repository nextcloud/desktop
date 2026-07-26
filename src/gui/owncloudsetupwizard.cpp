/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "owncloudsetupwizard.h"

#include "accountmanager.h"
#include "folderman.h"
#include "systray.h"
#include "theme.h"
#include "wizard/accountwizardcontroller.h"

#ifdef Q_OS_MACOS
#include "foregroundbackground_interface.h"
#include "nativetitlebar_mac.h"
#endif

#ifdef BUILD_FILE_PROVIDER_MODULE
#include "macOS/fileprovidersettingscontroller.h"
#endif

#include <QDialog>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QVariant>

namespace OCC {

Q_LOGGING_CATEGORY(lcWizard, "nextcloud.gui.wizard", QtInfoMsg)

OwncloudSetupWizard::OwncloudSetupWizard(QObject *parent)
    : QObject(parent)
{
#ifdef BUILD_FILE_PROVIDER_MODULE
    // The wizard decides up front whether to offer classic sync options based on the
    // app-level File Provider mode. If that mode is toggled from the General settings
    // while the wizard is open, its decision is stale — cancel the wizard so it is
    // restarted against the new mode, rather than provisioning a conflicting mix of a
    // classic sync folder and a File Provider domain. fileProviderModeEnabledChanged is
    // only emitted on an actual app-level toggle, never by the wizard's own account
    // creation (that emits the per-account vfsEnabledForAccountChanged).
    connect(Mac::FileProviderSettingsController::instance(),
            &Mac::FileProviderSettingsController::fileProviderModeEnabledChanged,
            this, [this] {
        if (!_finished) {
            qCInfo(lcWizard) << "File Provider mode changed while the account wizard was open; cancelling the wizard.";
            finish(QDialog::Rejected);
        }
    });
#endif
}

OwncloudSetupWizard::~OwncloudSetupWizard()
{
    if (_qmlWizardWindow) {
        _qmlWizardWindow->deleteLater();
    }
}

static QPointer<OwncloudSetupWizard> owncloudSetupWizard = nullptr;

void OwncloudSetupWizard::runWizard(QObject *obj, const char *amember, QWidget *parent, bool forceRestart)
{
    if (!owncloudSetupWizard.isNull()) {
        if (forceRestart) {
            owncloudSetupWizard->finish(QDialog::Rejected);
        } else {
            bringWizardToFrontIfVisible();
            return;
        }
    }

    if (!owncloudSetupWizard.isNull()) {
        bringWizardToFrontIfVisible();
        return;
    }

    owncloudSetupWizard = new OwncloudSetupWizard(parent);
    connect(owncloudSetupWizard, SIGNAL(ownCloudWizardDone(int)), obj, amember);

    FolderMan::instance()->setSyncEnabled(false);
    if (owncloudSetupWizard->startQmlWizard()) {
        return;
    }

    emit owncloudSetupWizard->ownCloudWizardDone(QDialog::Rejected);
    owncloudSetupWizard->deleteLater();
    owncloudSetupWizard.clear();
}

void OwncloudSetupWizard::runWizardForLoginFlow(QObject *obj, const char *amember, const QUrl &serverUrl, QWidget *parent)
{
#if defined ENFORCE_SINGLE_ACCOUNT
    if (!AccountManager::instance()->accounts().isEmpty()) {
        return;
    }
#endif

    if (!owncloudSetupWizard.isNull()) {
        qCInfo(lcWizard) << "Restarting existing setup wizard for URI login flow.";
        owncloudSetupWizard->finish(QDialog::Rejected);
    }

    if (!owncloudSetupWizard.isNull()) {
        bringWizardToFrontIfVisible();
        return;
    }

    owncloudSetupWizard = new OwncloudSetupWizard(parent);
    connect(owncloudSetupWizard, SIGNAL(ownCloudWizardDone(int)), obj, amember);

    FolderMan::instance()->setSyncEnabled(false);
    if (owncloudSetupWizard->startQmlWizardForLoginFlow(serverUrl)) {
        return;
    }

    emit owncloudSetupWizard->ownCloudWizardDone(QDialog::Rejected);
    owncloudSetupWizard->deleteLater();
    owncloudSetupWizard.clear();
}

bool OwncloudSetupWizard::bringWizardToFrontIfVisible()
{
    if (owncloudSetupWizard.isNull()
        || !owncloudSetupWizard->_qmlWizardWindow
        || owncloudSetupWizard->_finished) {
        return false;
    }

    if (!owncloudSetupWizard->_qmlWizardWindow->isVisible()) {
        owncloudSetupWizard->finish(QDialog::Rejected);
        return false;
    }

    owncloudSetupWizard->_qmlWizardWindow->show();
    owncloudSetupWizard->_qmlWizardWindow->raise();
    owncloudSetupWizard->_qmlWizardWindow->requestActivate();
    return true;
}

bool OwncloudSetupWizard::startQmlWizard()
{
    auto *engine = Systray::instance()->trayEngine();
    if (!engine) {
        qCWarning(lcWizard) << "Cannot start QML account wizard without a QML engine.";
        return false;
    }

    _qmlController = new AccountWizardController(this);
    QQmlComponent component(engine, QStringLiteral("qrc:/qml/src/gui/wizard/qml/AccountWizardWindow.qml"));
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("controller"), QVariant::fromValue<QObject *>(_qmlController));
    auto *createdObject = component.createWithInitialProperties(initialProperties);

    if (component.isError()) {
        qCWarning(lcWizard) << "Failed to load QML account wizard:" << component.errors();
    }

    _qmlWizardWindow = qobject_cast<QQuickWindow *>(createdObject);
    if (!_qmlWizardWindow) {
        if (createdObject) {
            createdObject->deleteLater();
        }
        _qmlController->deleteLater();
        _qmlController = nullptr;
        return false;
    }

    _qmlWizardWindow->setIcon(Theme::instance()->applicationIcon());

#ifdef Q_OS_MACOS
    auto *fgbg = new ForegroundBackground(this);
    _qmlWizardWindow->installEventFilter(fgbg);
#endif

    connect(_qmlController, &AccountWizardController::finished, this, &OwncloudSetupWizard::finish);
    connect(_qmlWizardWindow, &QQuickWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            finish(QDialog::Rejected);
        }
    });
    connect(_qmlWizardWindow, &QObject::destroyed, this, [this] {
        finish(QDialog::Rejected);
    });

    _qmlWizardWindow->show();
    _qmlWizardWindow->raise();
    _qmlWizardWindow->requestActivate();

#ifdef Q_OS_MACOS
    styleNativeTitleBar(_qmlWizardWindow, /*hideTitleText=*/true);
    // Re-apply when the window colour changes (e.g. the macOS light/dark switch) so the title bar
    // keeps matching the body. Updating it in the same synchronous step lets the system appearance
    // cross-fade animate the title bar together with the window content.
    connect(_qmlWizardWindow, &QQuickWindow::colorChanged, this, [this] {
        styleNativeTitleBar(_qmlWizardWindow, /*hideTitleText=*/true);
    });
#endif

    return true;
}

bool OwncloudSetupWizard::startQmlWizardForLoginFlow(const QUrl &serverUrl)
{
    auto *engine = Systray::instance()->trayEngine();
    if (!engine) {
        qCWarning(lcWizard) << "Cannot start QML account wizard without a QML engine.";
        return false;
    }

    _qmlController = new AccountWizardController(this);
    if (!_qmlController->setServerUrlForLoginFlow(serverUrl)) {
        qCWarning(lcWizard) << "Cannot start QML account wizard for URI login flow with an unconfigured server URL.";
        _qmlController->deleteLater();
        _qmlController = nullptr;
        return false;
    }
    _qmlController->submitServerUrl();

    QQmlComponent component(engine, QStringLiteral("qrc:/qml/src/gui/wizard/qml/AccountWizardWindow.qml"));
    QVariantMap initialProperties;
    initialProperties.insert(QStringLiteral("controller"), QVariant::fromValue<QObject *>(_qmlController));
    auto *createdObject = component.createWithInitialProperties(initialProperties);

    if (component.isError()) {
        qCWarning(lcWizard) << "Failed to load QML account wizard:" << component.errors();
    }

    _qmlWizardWindow = qobject_cast<QQuickWindow *>(createdObject);
    if (!_qmlWizardWindow) {
        if (createdObject) {
            createdObject->deleteLater();
        }
        _qmlController->deleteLater();
        _qmlController = nullptr;
        return false;
    }

    _qmlWizardWindow->setIcon(Theme::instance()->applicationIcon());

#ifdef Q_OS_MACOS
    auto *fgbg = new ForegroundBackground(this);
    _qmlWizardWindow->installEventFilter(fgbg);
#endif

    connect(_qmlController, &AccountWizardController::finished, this, &OwncloudSetupWizard::finish);
    connect(_qmlWizardWindow, &QQuickWindow::visibleChanged, this, [this](bool visible) {
        if (!visible) {
            finish(QDialog::Rejected);
        }
    });
    connect(_qmlWizardWindow, &QObject::destroyed, this, [this] {
        finish(QDialog::Rejected);
    });

    _qmlWizardWindow->show();
    _qmlWizardWindow->raise();
    _qmlWizardWindow->requestActivate();

#ifdef Q_OS_MACOS
    styleNativeTitleBar(_qmlWizardWindow, /*hideTitleText=*/true);
    connect(_qmlWizardWindow, &QQuickWindow::colorChanged, this, [this] {
        styleNativeTitleBar(_qmlWizardWindow, /*hideTitleText=*/true);
    });
#endif

    return true;
}

void OwncloudSetupWizard::finish(int result)
{
    if (_finished) {
        return;
    }

    _finished = true;
    Systray::instance()->setIsOpen(false);
    emit ownCloudWizardDone(result);
    if (_qmlWizardWindow) {
        _qmlWizardWindow->deleteLater();
        _qmlWizardWindow.clear();
    }
    owncloudSetupWizard.clear();
    deleteLater();
}

} // namespace OCC
