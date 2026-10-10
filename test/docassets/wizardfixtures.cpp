/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "wizardfixtures.h"
#include "accountwizardcontrollertestaccess.h"
#include "capturescene.h"
#include "catalogue.h"
#include "configfile.h"
#include <QQuickWindow>

namespace OCC::DocAssets
{
bool prepareWizard(CaptureScene &scene, const QString &scenario, QString *error)
{
    const auto fileProvider = scenario == QStringLiteral("wizard-sync-file-provider");
    ConfigFile().setMacFileProviderModeEnabled(fileProvider);
    auto *controller = new AccountWizardController(&scene);
    if (fileProvider && !controller->isUsingFileProvider()) {
        *error = QStringLiteral("File Provider is not supported by this build or operating system");
        return false;
    }
    auto step = AccountWizardController::ServerStep;
    if (scenario == QStringLiteral("wizard-browser-auth")) {
        step = AccountWizardController::BrowserAuthStep;

    } else if (scenario.startsWith(QStringLiteral("wizard-sync-")) || scenario == QStringLiteral("wizard-advanced-options")) {
        step = AccountWizardController::SyncOptionsStep;
    }
    AccountWizardControllerTestAccess::prepare(*controller, step);
    scene.source = windowSource;
    scene.properties.insert(QStringLiteral("controller"), QVariant::fromValue<QObject *>(controller));
    scene.ready = [controller, step](QString *failure) {
        if (AccountWizardControllerTestAccess::hasLiveServices(*controller) || controller->currentStep() != step || controller->busy()) {
            *failure = QStringLiteral("Wizard left the isolated fixture state");
            return false;
        }
        return true;
    };
    scene.expectsPopup = scenario == QStringLiteral("wizard-advanced-options") || scenario == QStringLiteral("wizard-proxy-settings")
        || scenario == QStringLiteral("wizard-client-certificate") || scenario == QStringLiteral("wizard-secure-connection");
    scene.activate = [controller, scenario](QQuickWindow *, QString *) {
        if (scenario == QStringLiteral("wizard-advanced-options")) {
            controller->openAdvancedOptions();
        } else if (scenario == QStringLiteral("wizard-proxy-settings")) {
            controller->openProxySettings();
        } else if (scenario == QStringLiteral("wizard-client-certificate")) {
            Q_EMIT controller->clientCertificateDialogRequested();
        } else if (scenario == QStringLiteral("wizard-secure-connection")) {
            Q_EMIT controller->secureConnectionFailed(QStringLiteral("cloud.example.com"), true);
        }
        return true;
    };
    return true;
}
}
