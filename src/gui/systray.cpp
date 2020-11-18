/*
 * Copyright (C) by Cédric Bellegarde <gnumdk@gmail.com>
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

#include "accountmanager.h"
#include "systray.h"
#include "theme.h"
#include "config.h"
#include "common/utility.h"
#include "tray/UserModel.h"

#include <QCursor>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QMenu>

#ifdef USE_FDO_NOTIFICATIONS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#define NOTIFICATIONS_SERVICE "org.freedesktop.Notifications"
#define NOTIFICATIONS_PATH "/org/freedesktop/Notifications"
#define NOTIFICATIONS_IFACE "org.freedesktop.Notifications"
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcSystray, "nextcloud.gui.systray")

Systray *Systray::_instance = nullptr;

Systray *Systray::instance()
{
    if (!_instance) {
        _instance = new Systray();
    }
    return _instance;
}

void Systray::setTrayEngine(QQmlApplicationEngine *trayEngine)
{
    _trayEngine = trayEngine;

    _trayEngine->addImportPath("qrc:/qml/theme");
    _trayEngine->addImageProvider("avatars", new ImageProvider);
}

Systray::Systray()
    : QSystemTrayIcon(nullptr)
{
    qmlRegisterSingletonType<UserModel>("com.nextcloud.desktopclient", 1, 0, "UserModel",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return UserModel::instance();
        }
    );

    qmlRegisterSingletonType<UserAppsModel>("com.nextcloud.desktopclient", 1, 0, "UserAppsModel",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return UserAppsModel::instance();
        }
    );

    qmlRegisterSingletonType<Systray>("com.nextcloud.desktopclient", 1, 0, "Theme",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return Theme::instance();
        }
    );

    qmlRegisterSingletonType<Systray>("com.nextcloud.desktopclient", 1, 0, "Systray",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return Systray::instance();
        }
    );

#ifndef Q_OS_MAC
    auto contextMenu = new QMenu();
    if (AccountManager::instance()->accounts().isEmpty()) {
        contextMenu->addAction(tr("Add account"), this, &Systray::openAccountWizard);
    } else {
        contextMenu->addAction(tr("Open main dialog"), this, &Systray::openMainDialog);
    }

    auto pauseAction = contextMenu->addAction(tr("Pause sync"), this, &Systray::slotPauseAllFolders);
    auto resumeAction = contextMenu->addAction(tr("Resume sync"), this, &Systray::slotUnpauseAllFolders);
    contextMenu->addAction(tr("Settings"), this, &Systray::openSettings);
    contextMenu->addAction(tr("Exit %1").arg(Theme::instance()->appNameGUI()), this, &Systray::shutdown);
    setContextMenu(contextMenu);

    connect(contextMenu, &QMenu::aboutToShow, [=] {
        const auto folders = FolderMan::instance()->map();

        const auto allPaused = std::all_of(std::cbegin(folders), std::cend(folders), [](Folder *f) { return f->syncPaused(); });
        const auto pauseText = folders.size() > 1 ? tr("Pause sync for all") : tr("Pause sync");
        pauseAction->setText(pauseText);
        pauseAction->setVisible(!allPaused);
        pauseAction->setEnabled(!allPaused);

        const auto anyPaused = std::any_of(std::cbegin(folders), std::cend(folders), [](Folder *f) { return f->syncPaused(); });
        const auto resumeText = folders.size() > 1 ? tr("Resume sync for all") : tr("Resume sync");
        resumeAction->setText(resumeText);
        resumeAction->setVisible(anyPaused);
        resumeAction->setEnabled(anyPaused);
    });
#endif

    connect(UserModel::instance(), &UserModel::newUserSelected,
        this, &Systray::slotNewUserSelected);

    connect(AccountManager::instance(), &AccountManager::accountAdded,
        this, &Systray::showWindow);
}

void Systray::create()
{
    if (_trayEngine) {
        if (!AccountManager::instance()->accounts().isEmpty()) {
            _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
        }
        _trayEngine->load(QStringLiteral("qrc:/qml/src/gui/tray/Window.qml"));
    }
    hideWindow();
    emit activated(QSystemTrayIcon::ActivationReason::Unknown);

    const auto folderMap = FolderMan::instance()->map();
    for (const auto *folder : folderMap) {
        if (!folder->syncPaused()) {
            _syncIsPaused = false;
            break;
        }
    }
}

void Systray::slotNewUserSelected()
{
    if (_trayEngine) {
        // Change ActivityModel
        _trayEngine->rootContext()->setContextProperty("activityModel", UserModel::instance()->currentActivityModel());
    }

    // Rebuild App list
    UserAppsModel::instance()->buildAppList();
}

void Systray::slotUnpauseAllFolders()
{
    setPauseOnAllFoldersHelper(false);
}

void Systray::slotPauseAllFolders()
{
    setPauseOnAllFoldersHelper(true);
}

void Systray::setPauseOnAllFoldersHelper(bool pause)
{
    // For some reason we get the raw pointer from Folder::accountState()
    // that's why we need a list of raw pointers for the call to contains
    // later on...
    const auto accounts = [=] {
        const auto ptrList = AccountManager::instance()->accounts();
        auto result = QList<AccountState *>();
        result.reserve(ptrList.size());
        std::transform(std::cbegin(ptrList), std::cend(ptrList), std::back_inserter(result), [](const AccountStatePtr &account) {
            return account.data();
        });
        return result;
    }();
    const auto folders = FolderMan::instance()->map();
    for (auto f : folders) {
        if (accounts.contains(f->accountState())) {
            f->setSyncPaused(pause);
            if (pause) {
                f->slotTerminateSync();
            }
        }
    }
}

bool Systray::isWindowVisible() const
{
    return _isWindowVisible;
}

void Systray::setWindowVisible(bool value)
{
    if (_isWindowVisible == value) {
        return;
    }

    _isWindowVisible = value;
    emit windowVisibleChanged(value);
}

bool Systray::isWindowActive() const
{
    return _isWindowActive;
}

void Systray::setWindowActive(bool value)
{
    if (_isWindowActive == value) {
        return;
    }

    _isWindowActive = value;
    emit windowActiveChanged(value);
}

void Systray::showMessage(const QString &title, const QString &message, MessageIcon icon)
{
#ifdef USE_FDO_NOTIFICATIONS
    if (QDBusInterface(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE).isValid()) {
        const QVariantMap hints = {{QStringLiteral("desktop-entry"), LINUX_APPLICATION_ID}};
        QList<QVariant> args = QList<QVariant>() << APPLICATION_NAME << quint32(0) << APPLICATION_ICON_NAME
                                                 << title << message << QStringList() << hints << qint32(-1);
        QDBusMessage method = QDBusMessage::createMethodCall(NOTIFICATIONS_SERVICE, NOTIFICATIONS_PATH, NOTIFICATIONS_IFACE, "Notify");
        method.setArguments(args);
        QDBusConnection::sessionBus().asyncCall(method);
    } else
#endif
#ifdef Q_OS_OSX
        if (canOsXSendUserNotification()) {
        sendOsXUserNotification(title, message);
    } else
#endif
    {
        QSystemTrayIcon::showMessage(title, message, icon);
    }
}

void Systray::setToolTip(const QString &tip)
{
    QSystemTrayIcon::setToolTip(tr("%1: %2").arg(Theme::instance()->appNameGUI(), tip));
}

bool Systray::syncIsPaused()
{
    return _syncIsPaused;
}

void Systray::pauseResumeSync()
{
    if (_syncIsPaused) {
        _syncIsPaused = false;
        slotUnpauseAllFolders();
    } else {
        _syncIsPaused = true;
        slotPauseAllFolders();
    }
}

} // namespace OCC
