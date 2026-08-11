/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "uiscreenshotrunner.h"

#include "configfile.h"
#include "nativescreenshotcontroller.h"
#include "qmlscreenshotcontroller.h"
#include "theme.h"
#include "uiscreenshotmode.h"
#include "uiscreenshotoutput.h"

#include <QApplication>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace OCC::UiScreenshots {

namespace {

[[nodiscard]] std::optional<int> reportFailure(const QString &message)
{
    fprintf(stderr, "%s\n", qUtf8Printable(message));
    return EXIT_FAILURE;
}

void configureApplication(QApplication &app, const QString &widgetsStyle)
{
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationDomain(QLatin1String(APPLICATION_REV_DOMAIN));
    app.setDesktopFileName(QString(LINUX_APPLICATION_ID));

    const auto theme = Theme::instance();
    app.setApplicationName(theme->appName());
    app.setWindowIcon(theme->applicationIcon());

    if (!widgetsStyle.isEmpty()) {
        QApplication::setStyle(QStyleFactory::create(widgetsStyle));
    }
}

std::unique_ptr<QTemporaryDir> createDisposableProfile(
    const UiScreenshotOutput &output,
    const QString &fileTemplate,
    QString *error)
{
    auto profile = std::make_unique<QTemporaryDir>(output.directory() + fileTemplate);
    if (!profile->isValid()) {
        if (error) {
            *error = QStringLiteral("Could not create the disposable screenshot profile.");
        }
        return {};
    }
    if (!ConfigFile::setConfDir(profile->path())) {
        if (error) {
            *error = QStringLiteral("Could not select the disposable screenshot profile.");
        }
        return {};
    }
    return profile;
}

template<typename Controller>
[[nodiscard]] int runController(QApplication &app, Controller &controller)
{
    QObject::connect(&controller, &Controller::finished, &app, [](const int exitCode) {
        QCoreApplication::exit(exitCode);
    });
    QTimer::singleShot(0, &controller, &Controller::start);
    return app.exec();
}

[[nodiscard]] std::optional<int> runQmlPhase(int &argc, char **argv, const QString &widgetsStyle)
{
    const auto outputDirectory = qEnvironmentVariable("NEXTCLOUD_UI_SCREENSHOT_OUTPUT");
    const auto screenshotOutput = UiScreenshotOutput(outputDirectory);
    auto error = QString{};
    if (!screenshotOutput.validate(&error)) {
        return reportFailure(error);
    }

    const auto screenshotProfile = createDisposableProfile(
        screenshotOutput, QStringLiteral("/.qml-profile-XXXXXX"), &error);
    if (!screenshotProfile) {
        return reportFailure(error);
    }

    auto app = QApplication(argc, argv);
    configureApplication(app, widgetsStyle);

    auto controller = QmlScreenshotController(outputDirectory);
    return runController(app, controller);
}

[[nodiscard]] std::optional<int> runNativePhase(int &argc, char **argv, const QString &widgetsStyle)
{
    auto output = UiScreenshotOutput(qEnvironmentVariable("NEXTCLOUD_UI_SCREENSHOT_OUTPUT"));
    const auto runId = qEnvironmentVariable("NEXTCLOUD_UI_SCREENSHOT_RUN_ID");
    auto error = QString{};
    if (!output.beginNativeRun(runId, &error)) {
        return reportFailure(error);
    }

    const auto screenshotProfile = createDisposableProfile(
        output, QStringLiteral("/.native-profile-XXXXXX"), &error);
    if (!screenshotProfile) {
        return reportFailure(error);
    }

    auto app = QApplication(argc, argv);
    configureApplication(app, widgetsStyle);
    ConfigFile().setMacFileProviderModeEnabled(!Theme::instance()->disableVirtualFilesSyncFolder());

    auto controller = NativeScreenshotController(output, runId);
    return runController(app, controller);
}

}

std::optional<int> runIfRequested(int &argc, char **argv, const QString &widgetsStyle)
{
    const auto parsedPhase = phaseFromEnvironment();
    if (parsedPhase.phase == Phase::None) {
        return std::nullopt;
    }

    qCInfo(lcUiScreenshots) << "Parsed screenshot phase" << phaseName(parsedPhase.phase);

    switch (parsedPhase.phase) {
    case Phase::None:
        return std::nullopt;
    case Phase::Qml:
        return runQmlPhase(argc, argv, widgetsStyle);
    case Phase::Native:
        return runNativePhase(argc, argv, widgetsStyle);
    case Phase::Invalid:
        return reportFailure(parsedPhase.error);
    }

    Q_UNREACHABLE();
    return EXIT_FAILURE;
}

}
