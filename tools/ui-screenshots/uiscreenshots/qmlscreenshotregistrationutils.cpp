/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qmlscreenshotregistrationutils.h"

#include "emojimodel.h"
#include "screenshotactivitymodel.h"
#include "screenshotsortedactivitylistmodel.h"
#include "screenshotsystray.h"
#include "screenshotsyncstatussummary.h"
#include "screenshotusermodel.h"
#include "screenshotuserstatusselectormodel.h"
#include "screenshotwizardcontroller.h"
#include "theme.h"
#include "tray/svgimageprovider.h"
#include "tray/usermodel.h"
#include "userstatusconnector.h"
#include "wheelhandler.h"
#include "wizard/accountwizardcontroller.h"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QtQml>

namespace OCC::UiScreenshots {

void registerQmlTypes(ScreenshotUserModel &userModel, ScreenshotSystray &systray)
{
    constexpr auto uri = "com.nextcloud.desktopclient";
    qmlRegisterType<WheelHandler>(uri, 1, 0, "WheelHandler");
    qmlRegisterType<EmojiModel>(uri, 1, 0, "EmojiModel");
    qmlRegisterType<ScreenshotSyncStatusSummary>(uri, 1, 0, "SyncStatusSummary");
    qmlRegisterType<ScreenshotSortedActivityListModel>(uri, 1, 0, "SortedActivityListModel");
    qmlRegisterType<ScreenshotUserStatusSelectorModel>(uri, 1, 0, "UserStatusSelectorModel");

    // Explicit screenshot names are used only by the development test harness.
    qmlRegisterType<ScreenshotActivityModel>(uri, 1, 0, "ScreenshotActivityModel");
    qmlRegisterType<ScreenshotWizardController>(uri, 1, 0, "ScreenshotWizardController");

    qmlRegisterUncreatableType<UserStatus>(uri, 1, 0, "userStatus", "Access to production status enum values");
    qmlRegisterUncreatableType<AccountWizardController>(uri, 1, 0, "AccountWizardController", "Access to production wizard enum values");

    QQmlEngine::setObjectOwnership(Theme::instance(), QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&userModel, QQmlEngine::CppOwnership);
    QQmlEngine::setObjectOwnership(&systray, QQmlEngine::CppOwnership);
    qmlRegisterSingletonInstance(uri, 1, 0, "Theme", Theme::instance());
    qmlRegisterSingletonInstance(uri, 1, 0, "UserModel", &userModel);
    qmlRegisterSingletonInstance(uri, 1, 0, "Systray", &systray);
}

void configureQmlEngine(QQmlApplicationEngine &engine)
{
    engine.addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
    engine.addImportPath(QStringLiteral("qrc:/qml/theme"));
    engine.addImageProvider(QStringLiteral("avatars"), new ImageProvider);
    engine.addImageProvider(QStringLiteral("svgimage-custom-color"), new Ui::SvgImageProvider);
}

}
