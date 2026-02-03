/*
 * SPDX-FileCopyrightText: 2018 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2011 ownCloud GmbH
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtGlobal>

#include <cmath>
#include <csignal>

#ifdef Q_OS_UNIX
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "application.h"
#include "cocoainitializer.h"
#include "theme.h"
#include "common/utility.h"

#if defined(BUILD_UPDATER)
#include "updater/updater.h"
#endif

#include <QTimer>
#include <QMessageBox>
#include <QDebug>
#include <QQuickStyle>
#include <QStyle>
#include <QStyleFactory>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QOperatingSystemVersion>

using namespace OCC;

void warnSystray()
{
    QMessageBox::critical(
        nullptr,
        qApp->translate("main.cpp", "System Tray not available"),
        qApp->translate("main.cpp", "%1 requires on a working system tray. "
                                    "If you are running XFCE, please follow "
                                    "<a href=\"http://docs.xfce.org/xfce/xfce4-panel/systray\">these instructions</a>. "
                                    "Otherwise, please install a system tray application such as \"trayer\" and try again.")
            .arg(Theme::instance()->appNameGUI()),
        QMessageBox::Ok
    );
}

int main(int argc, char **argv)
{
#ifdef Q_OS_LINUX
    const auto appImagePath = qEnvironmentVariable("APPIMAGE");
    const auto runningInsideAppImage = !appImagePath.isNull() && QFile::exists(appImagePath);
    if (runningInsideAppImage) {
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu-compositing");
    }
#endif

#ifdef Q_OS_WIN
    SetDllDirectory(L"");
    qputenv("QML_IMPORT_PATH", (QDir::currentPath() + QStringLiteral("/qml")).toLatin1());
#endif

    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);

    // OpenSSL 1.1.0: No explicit initialisation or de-initialisation is necessary.
#ifdef Q_OS_MACOS
    Mac::CocoaInitializer cocoaInit; // RIIA
#endif

    auto surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setOption(QSurfaceFormat::ResetNotification);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    auto qmlStyle = QStringLiteral("Fusion");
    auto widgetsStyle = QStringLiteral("");

#if defined Q_OS_MACOS
    qmlStyle = QStringLiteral("macOS");
#elif defined Q_OS_WIN
    if (const auto osVersion = QOperatingSystemVersion::current().version(); osVersion < QOperatingSystemVersion::Windows11.version()) {
        qmlStyle = QStringLiteral("Universal");
        widgetsStyle = QStringLiteral("Fusion");
        if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_UNIVERSAL_THEME")) {
            // initialise theme with the light/dark mode setting from the OS
            qputenv("QT_QUICK_CONTROLS_UNIVERSAL_THEME", "System");
        }

        if (osVersion < QOperatingSystemVersion::Windows10_1809.version() && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            // for Windows Server 2016 to display text as expected, see #8064
            qputenv("QT_QPA_PLATFORM", "windows:nodirectwrite");
        }
    } else {
        qmlStyle = QStringLiteral("FluentWinUI3");
        widgetsStyle = QStringLiteral("windows11");
    }
#endif

    QQuickStyle::setStyle(qmlStyle);

    OCC::Application app(argc, argv);

    if (!widgetsStyle.isEmpty()) {
        QApplication::setStyle(QStyleFactory::create(widgetsStyle));
    }

#ifndef Q_OS_WIN
    signal(SIGPIPE, SIG_IGN);
#endif
    if (app.giveHelp()) {
        app.showHelp();
        return 0;
    }
    if (app.versionOnly()) {
        app.showVersion();
        return 0;
    }

// check a environment variable for core dumps
#ifdef Q_OS_UNIX
    if (!qEnvironmentVariableIsEmpty("OWNCLOUD_CORE_DUMP")) {
        struct rlimit core_limit{};
        core_limit.rlim_cur = RLIM_INFINITY;
        core_limit.rlim_max = RLIM_INFINITY;

        if (setrlimit(RLIMIT_CORE, &core_limit) < 0) {
            fprintf(stderr, "Unable to set core dump limit\n");
        } else {
            qCInfo(lcApplication) << "Core dumps enabled";
        }
    }
#endif

#if defined(BUILD_UPDATER)
    // if handleStartup returns true, main()
    // needs to terminate here, e.g. because
    // the updater is triggered
    Updater *updater = Updater::instance();
    if (updater && updater->handleStartup()) {
        return 1;
    }
#endif

    // if the application is already running, notify it.
    if (app.isRunning()) {
        qCInfo(lcApplication) << "Already running, exiting...";
        if (app.isSessionRestored()) {
            // This call is mirrored with the one in Application::slotParseMessage
            qCInfo(lcApplication) << "Session was restored, don't notify app!";
            return -1;
        }

        QStringList args = app.arguments();
        if (args.size() > 1) {
            QString msg = args.join(QLatin1String("|"));
            if (!app.sendMessage(QLatin1String("MSG_PARSEOPTIONS:") + msg))
                return -1;
        } else if (!app.backgroundMode() && !app.sendMessage(QLatin1String("MSG_SHOWMAINDIALOG"))) {
            return -1;
        }
        return 0;
    }

    // We can't call isSystemTrayAvailable with appmenu-qt5 begause it hides the systemtray
    // (issue #4693)
    if (qgetenv("QT_QPA_PLATFORMTHEME") != "appmenu-qt5")
    {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            // If the systemtray is not there, we will wait one second for it to maybe start
            // (eg boot time) then we show the settings dialog if there is still no systemtray.
            // On XFCE however, we show a message box with explainaition how to install a systemtray.
            qCInfo(lcApplication) << "System tray is not available, waiting...";
            Utility::sleep(1);

            auto desktopSession = qgetenv("XDG_CURRENT_DESKTOP").toLower();
            if (desktopSession.isEmpty()) {
                desktopSession = qgetenv("DESKTOP_SESSION").toLower();
            }
            if (desktopSession == "xfce") {
                int attempts = 0;
                while (!QSystemTrayIcon::isSystemTrayAvailable()) {
                    attempts++;
                    if (attempts >= 30) {
                        qCWarning(lcApplication) << "System tray unavailable (xfce)";
                        warnSystray();
                        break;
                    }
                    Utility::sleep(1);
                }
            }

            if (QSystemTrayIcon::isSystemTrayAvailable()) {
                app.tryTrayAgain();
            } else if (!app.backgroundMode() && !AccountManager::instance()->accounts().isEmpty()) {
                if (desktopSession != "ubuntu") {
                    qCInfo(lcApplication) << "System tray still not available, showing window and trying again later";
                    app.showMainDialog();
                    QTimer::singleShot(10000, &app, &Application::tryTrayAgain);
                } else {
                    qCInfo(lcApplication) << "System tray still not available, but assuming it's fine on 'ubuntu' desktop";
                }
            }
        }
    }

    return app.exec();
}
