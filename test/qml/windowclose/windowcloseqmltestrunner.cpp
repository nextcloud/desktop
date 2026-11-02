/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/activity/sortedactivitylistmodel.h"
#include "gui/activity/syncstatussummary.h"
#include "gui/fileactivitylistmodel.h"
#include "gui/filedetails/datefieldbackend.h"
#include "gui/filedetails/filedetails.h"
#include "gui/filedetails/shareemodel.h"
#include "gui/filedetails/sharemodel.h"
#include "gui/filedetails/sortedsharemodel.h"
#include "gui/folderman.h"
#include "gui/systray.h"
#include "gui/tray/usermodel.h"
#include "theme.h"
#include "wheelhandler.h"

#include <QApplication>
#include <QKeySequence>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>
#include <QtQuickTest/quicktest.h>

class WindowCloseTestHelper final : public QObject
{
    Q_OBJECT

public:
    /** @brief Reports whether the runner supports QWidget-based tray menus. */
    Q_INVOKABLE [[nodiscard]] bool usesWidgetApplication() const
    {
        return qobject_cast<QApplication *>(QCoreApplication::instance()) != nullptr;
    }

    Q_INVOKABLE bool closeShortcut(QQuickWindow *window)
    {
        const auto closeShortcuts = QKeySequence::keyBindings(QKeySequence::Close);
        if (closeShortcuts.isEmpty()) {
            return false;
        }

        const auto closeShortcut = closeShortcuts.constFirst();
        const auto closeKey = closeShortcut[0];
        QTest::keyClick(window, closeKey.key(), closeKey.keyboardModifiers());
        return true;
    }
};

class WindowCloseQmlTestSetup final : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        Q_INIT_RESOURCE(resources);
        Q_INIT_RESOURCE(theme);
        QStandardPaths::setTestModeEnabled(true);
        qmlRegisterType<OCC::FileActivityListModel>("com.nextcloud.desktopclient", 1, 0, "FileActivityListModel");
        qmlRegisterType<OCC::Quick::DateFieldBackend>("com.nextcloud.desktopclient", 1, 0, "DateFieldBackend");
        qmlRegisterType<OCC::FileDetails>("com.nextcloud.desktopclient", 1, 0, "FileDetails");
        qmlRegisterType<OCC::ShareModel>("com.nextcloud.desktopclient", 1, 0, "ShareModel");
        qmlRegisterType<OCC::ShareeModel>("com.nextcloud.desktopclient", 1, 0, "ShareeModel");
        qmlRegisterType<OCC::SortedShareModel>("com.nextcloud.desktopclient", 1, 0, "SortedShareModel");
        qmlRegisterType<OCC::SortedActivityListModel>("com.nextcloud.desktopclient", 1, 0, "SortedActivityListModel");
        qmlRegisterType<OCC::SyncStatusSummary>("com.nextcloud.desktopclient", 1, 0, "SyncStatusSummary");
        qmlRegisterType<WheelHandler>("com.nextcloud.desktopclient", 1, 0, "WheelHandler");

        OCC::FolderMan::instance();
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "UserModel", OCC::UserModel::instance());
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Theme", OCC::Theme::instance());
        qmlRegisterSingletonInstance("com.nextcloud.desktopclient", 1, 0, "Systray", OCC::Systray::instance());
        OCC::Systray::instance()->createTrayEngine();

        engine->addImportPath(QCoreApplication::applicationDirPath());
        engine->addImportPath(QCoreApplication::applicationDirPath() + QStringLiteral("/qml"));
        engine->addImportPath(QStringLiteral("qrc:/qml/theme"));
        engine->rootContext()->setContextProperty(QStringLiteral("windowCloseTestHelper"), &_helper);
    }

private:
    WindowCloseTestHelper _helper;
};

int main(int argc, char **argv)
{
    QTEST_SET_MAIN_SOURCE_PATH
    // The tray singleton creates QWidget-based menus on Linux and Windows.
    auto app = QApplication(argc, argv);
    auto setup = WindowCloseQmlTestSetup();
    return quick_test_main_with_setup(argc, argv, "windowclose", nullptr, &setup);
}

#include "windowcloseqmltestrunner.moc"
