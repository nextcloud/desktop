/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "setup.h"
#include "activity/sortedactivitylistmodel.h"
#include "activity/syncstatussummary.h"
#include "assistant/assistantcontroller.h"
#include "configfile.h"
#include "emojimodel.h"
#include "filedetails/filedetails.h"
#include "fixtureuserstatusmodel.h"
#include "theme.h"
#include "tray/usermodel.h"
#include "wheelhandler.h"
#include "wizard/accountwizardcontroller.h"
#include <QLocale>
#include <QQmlEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStandardPaths>
#include <QSurfaceFormat>

namespace OCC::DocAssets
{
bool prepareEnvironment(const QString &configurationDirectory)
{
    if (!ConfigFile::setConfDir(configurationDirectory)) {
        return false;
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, configurationDirectory);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, configurationDirectory);
    QStandardPaths::setTestModeEnabled(true);
    qputenv("QML_DISABLE_DISK_CACHE", "1");
    qputenv("QT_SHADER_CACHE_PATH", configurationDirectory.toUtf8());
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    auto format = QSurfaceFormat::defaultFormat();
    format.setOption(QSurfaceFormat::ResetNotification);
    QSurfaceFormat::setDefaultFormat(format);
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
    QQuickStyle::setStyle(QStringLiteral("macOS"));
    return true;
}

void registerWizardTypes()
{
    qmlRegisterUncreatableType<AssistantController>("com.nextcloud.desktopclient", 1, 0, "AssistantController", "Supplied by the capture runner");
    qmlRegisterUncreatableType<QAbstractItemModel>("com.nextcloud.desktopclient", 1, 0, "QAbstractItemModel", "Supplied by the capture runner");
    qmlRegisterUncreatableType<Activity>("com.nextcloud.desktopclient", 1, 0, "activity", "Activity");
    qmlRegisterType<SyncStatusSummary>("com.nextcloud.desktopclient", 1, 0, "SyncStatusSummary");
    qmlRegisterType<SortedActivityListModel>("com.nextcloud.desktopclient", 1, 0, "SortedActivityListModel");
    qmlRegisterType<WheelHandler>("com.nextcloud.desktopclient", 1, 0, "WheelHandler");
    qmlRegisterType<EmojiModel>("com.nextcloud.desktopclient", 1, 0, "EmojiModel");
    qmlRegisterType<FileDetails>("com.nextcloud.desktopclient", 1, 0, "FileDetails");
    qmlRegisterType<FixtureUserStatusModel>("com.nextcloud.desktopclient", 1, 0, "UserStatusSelectorModel");
    qmlRegisterUncreatableType<UserStatus>("com.nextcloud.desktopclient", 1, 0, "userStatus", "Access to Status enum");
    // Keep registration local to this executable, as in the existing QML test runners.
    qmlRegisterUncreatableType<AccountWizardController>("com.nextcloud.desktopclient", 1, 0, "AccountWizardController", "Supplied by the capture runner");
    // Tests create engines sequentially; singleton-instance registration permits only one engine.
    qmlRegisterSingletonType<Theme>("com.nextcloud.desktopclient", 1, 0, "Theme", [](QQmlEngine *, QJSEngine *) {
        auto *theme = Theme::instance();
        QQmlEngine::setObjectOwnership(theme, QQmlEngine::CppOwnership);
        return theme;
    });
    qmlRegisterSingletonType<UserModel>("com.nextcloud.desktopclient", 1, 0, "UserModel", [](QQmlEngine *, QJSEngine *) {
        auto *model = UserModel::instance();
        QQmlEngine::setObjectOwnership(model, QQmlEngine::CppOwnership);
        return model;
    });
    Theme::instance()->setOverrideServerUrl({});
    Theme::instance()->setForceOverrideServerUrl(false);
    Theme::instance()->setStartLoginFlowAutomatically(false);
}
}
