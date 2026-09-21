/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "arguments.h"
#include "capture.h"
#include "captureguard.h"
#include "cocoainitializer.h"
#include "exporter.h"
#include "iconexport.h"
#include "logger.h"
#include "outputdirectory.h"
#include "setup.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QStyleHints>
#include <QTemporaryDir>

Q_LOGGING_CATEGORY(lcDocAssets, "nextcloud.docassets")

int main(int argc, char **argv)
{
    using namespace OCC::DocAssets;
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);
    Q_INIT_RESOURCE(assistant);
    OCC::Mac::CocoaInitializer cocoa;
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
    QString error;
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
    qCInfo(lcDocAssets).noquote() << "Exported" << captureScenarios().size() << "screenshots and" << statusIconSources().size()
                                  << "status and tray icons (PNGs) to" << destination;
    return 0;
}
