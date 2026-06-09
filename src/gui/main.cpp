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

namespace {
constexpr int InitialTrayWaitSeconds = 1;
constexpr int XfceTrayMaxAttempts = 30;
constexpr int DelayedTrayRetryMs = 10'000;

QByteArray currentDesktopSession()
{
    const auto xdgCurrentDesktopEnv = qgetenv("XDG_CURRENT_DESKTOP");
    const auto desktopSessionEnv = qgetenv("DESKTOP_SESSION");

    effective = xdgCurrentDesktopEnv.toLower();
    if (effective.isEmpty()) {
        effective = desktopSessionEnv.toLower();
    }

    qCInfo(lcApplication) << "Tray availability check:"
                      << "XDG_CURRENT_DESKTOP=" << xdgCurrentDesktopEnv
                      << "DESKTOP_SESSION=" << desktopSessionEnv
                      << "effective=" << effective;

    return effective;
}

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

enum class RunningInstanceResult {
    ContinueStartup = 1, // No running instance detected
    ExitHandled = 0,     // Existing instance handled or intentionally ignored startup
    ExitError = -1,      // Existing instance detected, but handoff failed
};

RunningInstanceResult handleRunningInstance(Application &app)
{
    if (!app.isRunning()) {
        qCDebug(lcApplication) << "No running instance detected; continuing startup.";
        return RunningInstanceResult::ContinueStartup;
    }

    qCInfo(lcApplication) << "Another instance is already running.";

    if (app.isSessionRestored()) {
        qCInfo(lcApplication) << "Session restore detected; not notifying the running instance.";
        return RunningInstanceResult::ExitHandled;
    }

    const QStringList args = app.arguments();
    if (args.size() > 1) {
        qCInfo(lcApplication) << "Forwarding startup arguments to the running instance.";
        const QString msg = args.join(QLatin1String("|"));
        if (app.sendMessage(QLatin1String("MSG_PARSEOPTIONS:") + msg)) {
            return RunningInstanceResult::ExitHandled;
        }

        qCWarning(lcApplication) << "Failed to forward startup arguments to the running instance.";
        return RunningInstanceResult::ExitError;
    }
    
    if (app.backgroundMode()) {
        // FIXME: background mode itself is requested via a startup argument... this is unreachable code (!)
        qCInfo(lcApplication) << "Background mode requested with no startup arguments; not requesting the running instance to show the main dialog.";
        return RunningInstanceResult::ExitHandled;
    }
    
    qCInfo(lcApplication) << "Requesting the running instance to show the main dialog.";
    // This call is mirrored with the one in Application::slotParseMessage
    if (app.sendMessage(QLatin1String("MSG_SHOWMAINDIALOG"))) {
        return RunningInstanceResult::ExitHandled;
    }

    qCWarning(lcApplication) << "Failed to request the main dialog from the running instance.";
    return RunningInstanceResult::ExitError;
}

// May block - depending on tray availability
void handleSystemTrayAvailability(Application &app)
{
    // Skip this check with appmenu-qt5 because that platform theme hides the system tray (#4693)
    if (qgetenv("QT_QPA_PLATFORMTHEME") == "appmenu-qt5") {
        return;
    }

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    const auto desktopSession = currentDesktopSession();
    
    // If the system tray is not there, we will wait one second for it to maybe start
    // (e.g. boot time) then we show the settings dialog if there is still no system tray.
    // On XFCE however, we show a message box with explanation how to install a system tray.

    qCInfo(lcApplication) << "System tray is not available, waiting...";
    Utility::sleep(InitialTrayWaitSeconds);

    if (desktopSession == "xfce") { // FIXME: This seems too strict; XDG_CURRENT_DESKTOP can be a composite
        int attempts = 0;
        // NOTE: This loop can block for up to 30 more seconds after the initial sleep (!)
        while (!QSystemTrayIcon::isSystemTrayAvailable() && attempts < XfceTrayMaxAttempts) {
            ++attempts;
            Utility::sleep(InitialTrayWaitSeconds);
        }

        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            qCWarning(lcApplication) << "System tray unavailable even after waiting 30s (xfce detected)";
            warnSystray();
            break;
        }
    }

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        app.tryTrayAgain();
        return;
    }

    if (app.backgroundMode() || AccountManager::instance()->accounts().isEmpty()) {
        return;
    }

    if (desktopSession == "ubuntu")) { // FIXME: This seems too strict; XDG_CURRENT_DESKTOP can be a composite (e.g. "ubuntu:gnome")
        // Ubuntu desktops may operate acceptably without reporting a traditional tray here.
        qCInfo(lcApplication) << "System tray still not available, but assuming it's fine on Ubuntu-like desktop";
        return;
    }

    qCInfo(lcApplication) << "System tray still not available, showing window and trying again later";
    app.showMainDialog();
    QTimer::singleShot(DelayedTrayRetryMs, &app, &Application::tryTrayAgain);
}
}

// ----------------------------------------------------------------------------------

int main(int argc, char **argv)
{
#ifdef Q_OS_LINUX
    const auto appImagePath = qEnvironmentVariable("APPIMAGE");
    const auto runningInsideAppImage = !appImagePath.isNull() && QFile::exists(appImagePath);

    if (runningInsideAppImage) {
        // Work around rendering issues seen when the client runs from an AppImage.
        qputenv("QTWEBENGINE_CHROMIUM_FLAGS", "--disable-gpu-compositing");
    }
#endif

#ifdef Q_OS_WIN
    // Avoid loading DLLs from the current working directory.
    SetDllDirectory(L"");
    // Ensure bundled QML modules are found when launching from the install directory.
    // FIXME: confirm the use of currentPath() is intentional here; seems like it
    // should be based on the executable path instead (to get install directory)?
    qputenv("QML_IMPORT_PATH", (QDir::currentPath() + QStringLiteral("/qml")).toLatin1());
#endif

    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);

#ifdef Q_OS_MACOS
    Mac::CocoaInitializer cocoaInit; // RIIA
#endif

    // Prevent context losses / corruptions with some OpenGL drivers (#4340)
    auto surfaceFormat = QSurfaceFormat::defaultFormat();
    surfaceFormat.setOption(QSurfaceFormat::ResetNotification);
    QSurfaceFormat::setDefaultFormat(surfaceFormat);

    // Native text rendering is believed to be less blurry (#2409);
    // may cause pixelation with advanced features (e.g. text transformation).
    // https://doc.qt.io/qt-6/qquickwindow.html#TextRenderType-enum
    QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);

    // Default styling (i.e. for Linux + UNIX)
    auto qmlStyle = QStringLiteral("Fusion");
    auto widgetsStyle = QStringLiteral("");

#if defined Q_OS_MACOS
    qmlStyle = QStringLiteral("macOS");
    widgetsStyle = QStringLiteral("");
#elif defined Q_OS_WIN
    const auto osVersion = QOperatingSystemVersion::current().version(); 
    if (osVersion < QOperatingSystemVersion::Windows11.version()) {
        // Use the older Qt Quick Controls style on pre-Windows 11 systems.
        qmlStyle = QStringLiteral("Universal");
        widgetsStyle = QStringLiteral("Fusion");
        if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_UNIVERSAL_THEME")) {
            // Initialise theme with the light/dark mode setting from the OS.
            qputenv("QT_QUICK_CONTROLS_UNIVERSAL_THEME", "System");
        }

        if (osVersion < QOperatingSystemVersion::Windows10_1809.version() && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
            // Work around DirectWrite-related text rendering issues on older Windows versions like Server 2016 (#8064).
            qputenv("QT_QPA_PLATFORM", "windows:nodirectwrite");
        }
    } else {
        // Match newer Windows styling more closely on Windows 11 and later.
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
    const auto runningInstanceResult = handleRunningInstance(app);
    if (runningInstanceResult != RunningInstanceResult::ContinueStartup) {
        return static_cast<int>(runningInstanceResult);
    }

    handleSystemTrayAvailability(app);

    return app.exec();
}
