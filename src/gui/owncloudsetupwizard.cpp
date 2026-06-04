/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2012 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "owncloudsetupwizard.h"

#include "folderman.h"
#include "systray.h"
#include "wizard/accountwizardcontroller.h"

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
}

OwncloudSetupWizard::~OwncloudSetupWizard()
{
    if (_qmlWizardWindow) {
        _qmlWizardWindow->deleteLater();
    }
}

static QPointer<OwncloudSetupWizard> owncloudSetupWizard = nullptr;

void OwncloudSetupWizard::runWizard(QObject *obj, const char *amember, QWidget *parent)
{
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

bool OwncloudSetupWizard::bringWizardToFrontIfVisible()
{
    if (owncloudSetupWizard.isNull() || !owncloudSetupWizard->_qmlWizardWindow) {
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

#if defined(Q_OS_MACOS) && QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    _qmlWizardWindow->setFlag(Qt::ExpandedClientAreaHint, true);
    _qmlWizardWindow->setFlag(Qt::NoTitleBarBackgroundHint, true);
#endif

    connect(_qmlController, &AccountWizardController::finished, this, [this](int result) {
        emit ownCloudWizardDone(result);
        if (_qmlWizardWindow) {
            _qmlWizardWindow->deleteLater();
            _qmlWizardWindow.clear();
        }
        owncloudSetupWizard.clear();
        deleteLater();
    });

    _qmlWizardWindow->show();
    _qmlWizardWindow->raise();
    _qmlWizardWindow->requestActivate();
    return true;
}

} // namespace OCC
