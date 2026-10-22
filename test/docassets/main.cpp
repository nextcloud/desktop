/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "application.h"
#include "arguments.h"
#include "capture.h"
#include "captureguard.h"
#include "cocoainitializer.h"
#include "configfile.h"
#include "exporter.h"
#include "iconexport.h"
#include "livecapture.h"
#include "liveprofile.h"
#include "logger.h"
#include "outputdirectory.h"
#include "setup.h"
#include "systray.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QQmlExtensionPlugin>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QStyleHints>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <vector>

Q_IMPORT_QML_PLUGIN(com_nextcloud_desktopclient_searchPlugin)
Q_IMPORT_QML_PLUGIN(com_nextcloud_desktopclient_sharingPlugin)

Q_LOGGING_CATEGORY(lcDocAssets, "nextcloud.docassets")

int main(int argc, char **argv)
{
    using namespace OCC::DocAssets;
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);
    Q_INIT_RESOURCE(assistant);
    OCC::Mac::CocoaInitializer cocoa;
    auto rawArguments = QStringList{};
    rawArguments.reserve(argc);
    for (auto index = 0; index < argc; ++index) {
        rawArguments.append(QString::fromLocal8Bit(argv[index]));
    }
    QString error;
    const auto normalizedArguments = normalizedCaptureArguments(rawArguments, &error);
    if (!error.isEmpty()) {
        qCritical().noquote() << error;
        return 2;
    }
    if (normalizedArguments.size() == 2 && normalizedArguments.at(1) == QStringLiteral("--check-platform")) {
        QApplication application(argc, argv);
        // Exercise the macOS API that aborts when the runner is outside an app bundle.
        OCC::setUserNotificationCenterDelegate();
        return 0;
    }
    if (normalizedArguments.size() == 4 && normalizedArguments.at(1) == QStringLiteral("--capture-worker")
        && scenarioUsesLiveProfile(normalizedArguments.at(2))) {
        if (captureFilename(normalizedArguments.at(2)).isEmpty() || !QDir::isAbsolutePath(normalizedArguments.at(3))
            || !QFileInfo(normalizedArguments.at(3)).isDir()) {
            qCritical() << "Invalid live capture scenario or staging directory";
            return 2;
        }
        const auto profile = savedDevProfile(&error);
        if (!profile) {
            qCritical().noquote() << error;
            return 2;
        }
        if (!OCC::ConfigFile::setConfDir(profile->directory)) {
            qCritical() << "Cannot select the NextcloudDev profile";
            return 1;
        }
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile->directory);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, profile->directory);
        qputenv("QML_DISABLE_DISK_CACHE", "1");
        qputenv("QT_SHADER_CACHE_PATH", profile->directory.toUtf8());
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
        auto surfaceFormat = QSurfaceFormat::defaultFormat();
        surfaceFormat.setOption(QSurfaceFormat::ResetNotification);
        QSurfaceFormat::setDefaultFormat(surfaceFormat);
        QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
        QQuickStyle::setStyle(QStringLiteral("macOS"));
        auto executable = QFile::encodeName(normalizedArguments.first());
        auto confdirOption = QByteArrayLiteral("--confdir");
        auto profilePath = QFile::encodeName(profile->directory);
        auto applicationArguments = std::vector<char *>{executable.data(), confdirOption.data(), profilePath.data(), nullptr};
        auto applicationArgumentCount = static_cast<int>(applicationArguments.size() - 1);
        OCC::Application application(applicationArgumentCount, applicationArguments.data());
        application.setQuitOnLastWindowClosed(false);
        application.setLayoutDirection(Qt::LeftToRight);
        application.styleHints()->setColorScheme(Qt::ColorScheme::Light);
        OCC::Logger::instance()->setLogFile(QStringLiteral("-"));
        OCC::Logger::instance()->setLogFlush(true);
        CaptureGuard guard;
        application.installEventFilter(&guard);
        QDesktopServices::setUrlHandler(QStringLiteral("https"), &guard, "consumeUrl");
        QDesktopServices::setUrlHandler(QStringLiteral("http"), &guard, "consumeUrl");
        if (application.isRunning() || !application.gui()) {
            qCritical() << "The capture client could not start. Quit every other Nextcloud client instance first.";
            return 1;
        }
        auto skipped = false;
        if (!captureLiveScenario(application, *profile, normalizedArguments.at(2), normalizedArguments.at(3), captureTimeoutMs, &error, &skipped)) {
            if (skipped) {
                qInfo().noquote() << "Skipped" << normalizedArguments.at(2) << error;
                return captureSkippedExitCode;
            }
            qCritical().noquote() << normalizedArguments.at(2) << error;
            return 1;
        }
        return 0;
    }
    QTemporaryDir configuration;
    if (!configuration.isValid() || !prepareEnvironment(configuration.path())) {
        qCCritical(lcDocAssets) << "Cannot create temporary capture configuration";
        return 1;
    }
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("NextcloudDocAssets"));
    app.setApplicationName(QStringLiteral("DocAssetsCapture"));
    app.setQuitOnLastWindowClosed(false);
    app.setLayoutDirection(Qt::LeftToRight);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    OCC::Logger::instance()->setLogFile(QStringLiteral("-"));
    OCC::Logger::instance()->setLogFlush(true);
    const auto arguments = normalizedCaptureArguments(app.arguments(), &error);
    if (!error.isEmpty()) {
        qCCritical(lcDocAssets) << error;
        return 2;
    }
    CaptureGuard guard;
    app.installEventFilter(&guard);
    QDesktopServices::setUrlHandler(QStringLiteral("https"), &guard, "consumeUrl");
    QDesktopServices::setUrlHandler(QStringLiteral("http"), &guard, "consumeUrl");
    registerWizardTypes();
    if (arguments.size() == 4 && arguments.at(1) == QStringLiteral("--capture-worker")) {
        if (!QDir::isAbsolutePath(arguments.at(3)) || !QDir(arguments.at(3)).exists()
            || !captureScenario(arguments.at(2), arguments.at(3), captureTimeoutMs, &error)) {
            qCCritical(lcDocAssets).noquote() << arguments.at(2) << error;
            return 1;
        }
        return 0;
    }
    if (arguments.size() == 2 && arguments.at(1) == QStringLiteral("--list")) {
        for (const auto &scenario : captureScenarios()) {
            qCInfo(lcDocAssets).noquote() << scenario;
        }
        for (const auto &omission : captureOmissions()) {
            qCInfo(lcDocAssets).noquote() << "Omitted:" << omission;
        }
        return 0;
    }
    auto destination = downloadsExportDirectory();
    if (arguments.size() == 2 && arguments.at(1) == QStringLiteral("--help")) {
        qCInfo(lcDocAssets) << "Usage: DocAssetsCapture [--output /absolute/new/folder]. Default: a new folder in Downloads.";
        return 0;
    }
    if (arguments.size() == 3 && arguments.at(1) == QStringLiteral("--output")) {
        destination = arguments.at(2);
    } else if (arguments.size() != 1) {
        qCCritical(lcDocAssets) << "Usage: DocAssetsCapture [--output /absolute/new/folder]";
        return 2;
    }
    if (destination.isEmpty() || !QDir::isAbsolutePath(destination)) {
        qCCritical(lcDocAssets) << "Could not resolve an absolute output directory";
        return 2;
    }
    if (!savedDevProfile(&error)) {
        qCCritical(lcDocAssets).noquote() << error;
        return 2;
    }
    if (arguments.size() == 1 && !QDir().mkpath(QFileInfo(destination).absolutePath())) {
        qCCritical(lcDocAssets) << "Cannot create Downloads directory";
        return 1;
    }
    for (const auto &omission : captureOmissions()) {
        qCInfo(lcDocAssets).noquote() << "Omitted:" << omission;
    }
    if (!exportCatalogue(app.applicationFilePath(), destination, captureScenarios(), &error, captureTimeoutMs + workerStartupTimeoutMs, true)) {
        qCCritical(lcDocAssets).noquote() << error;
        return 1;
    }
    const auto screenshots = QDir(destination).entryList({QStringLiteral("*.png")}, QDir::Files).size() - statusIconSources().size();
    qCInfo(lcDocAssets).noquote() << "Exported" << screenshots << "screenshots and" << statusIconSources().size() << "status and tray icons (PNGs) to"
                                  << destination;
    return 0;
}
