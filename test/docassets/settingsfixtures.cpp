/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "settingsfixtures.h"
#include "accountsettings.h"
#include "accountstate.h"
#include "capturescene.h"
#include "fixtureaccount.h"
#include "fixturefolderstatusmodel.h"
#include "fixturenetworkaccessmanager.h"
#include "generalsettings.h"
#include "ignorelisteditor.h"
#include "ignorelisttablewidget.h"
#include "infosettings.h"
#include "settingsdialog.h"
#include <QAction>
namespace OCC::DocAssets
{
GeneralSettingsServices settingsServices()
{
    GeneralSettingsServices services;
    services.systemAutoStart = [] {
        return false;
    };
    services.autoStart = [] {
        return false;
    };
    services.setAutoStart = [](bool) { };
    services.fileProviderEnabled = [] {
        return false;
    };
    services.fileProviderBusy = [] {
        return false;
    };
    services.observeFileProvider = [](QObject *, const std::function<void()> &) { };
    return services;
}
bool prepareSettings(CaptureScene &scene, const QString &scenario, QString *error)
{
    if (scenario != QStringLiteral("settings-account-classic") && scenario != QStringLiteral("settings-general")
        && scenario != QStringLiteral("settings-advanced") && scenario != QStringLiteral("settings-ignored-files")
        && scenario != QStringLiteral("settings-info")) {
        *error = QStringLiteral("Unsupported Settings scenario: %1").arg(scenario);
        return false;
    }
    if (scenario == QStringLiteral("settings-ignored-files")) {
        auto editor = std::make_unique<IgnoreListEditor>(nullptr, true);
        const auto table = editor->findChild<IgnoreListTableWidget *>();
        if (!table) {
            *error = QStringLiteral("Ignored Files table is unavailable");
            return false;
        }
        table->addPattern(QStringLiteral("*.tmp"), false, false);
        table->addPattern(QStringLiteral("build/"), false, false);
        scene.widget = std::move(editor);
        return true;
    }
    auto dialog = std::make_unique<SettingsDialog>(
        nullptr,
        nullptr,
        [](QWidget *parent) {
            return new GeneralSettings(parent, settingsServices());
        },
        [](QWidget *parent) {
            return new InfoSettings(parent, []() -> Updater * {
                return nullptr;
            });
        },
        [] {
            return true;
        });
    auto *window = dialog.get();
    if (scenario == QStringLiteral("settings-account-classic")) {
        prepareAccount(scene);
        auto *network = static_cast<FixtureNetworkAccessManager *>(scene.account->networkAccessManager());
        network->getResponses.insert(
            QUrl(QStringLiteral("https://cloud.example.com/ocs/v2.php/apps/user_status/api/v1/user_status?format=json")),
            QByteArrayLiteral(
                R"({"ocs":{"meta":{"status":"ok","statuscode":200},"data":{"userId":"alex","status":"online","message":"Reviewing the project plan","icon":"","messageIsPredefined":false,"clearAt":null}}})"));
        scene.account->setCapabilities({
            {QStringLiteral("assistant"), QVariantMap{{QStringLiteral("enabled"), true}, {QStringLiteral("version"), QStringLiteral("1.0.9")}}},
            {QStringLiteral("user_status"), QVariantMap{{QStringLiteral("enabled"), true}}},
        });
        auto services = AccountSettingsServices{};
        services.model = new FixtureFolderStatusModel;
        services.folders = [] {
            return QList<Folder *>{};
        };
        services.fileProviderEnabled = [] {
            return false;
        };
        services.setUserInfoActive = [](bool) { };
        services.initializeEncryption = [] { };
        auto *page = new AccountSettings(scene.accountState.data(), window, services);
        constexpr qint64 gibibyte = 1024 * 1024 * 1024;
        page->slotUpdateQuota(100 * gibibyte, 12 * gibibyte);
        window->addAccountPage(scene.accountState.data(), page, false);
        window->adjustSize();
        scene.activate = [window, state = scene.accountState.data()](QQuickWindow *, QString *) {
            window->showAccount(state);
            return true;
        };
        scene.settled = [window, page](QQuickWindow *, QString *) {
            return window->currentPage() == page;
        };
        scene.widget = std::move(dialog);
        return true;
    }
    const auto page = scenario == QStringLiteral("settings-general") ? QStringLiteral("General")
        : scenario == QStringLiteral("settings-advanced")            ? QStringLiteral("Advanced")
                                                                     : QStringLiteral("Info");
    scene.activate = [window, page](QQuickWindow *, QString *) {
        for (auto *action : window->findChildren<QAction *>()) {
            if (action->text() == page) {
                action->trigger();
                return true;
            }
        }
        return false;
    };
    scene.settled = [window, page](QQuickWindow *, QString *) {
        return window->currentPage() && window->currentPage()->objectName() == QStringLiteral("settingsPage_") + page;
    };
    scene.widget = std::move(dialog);
    return true;
}
}
