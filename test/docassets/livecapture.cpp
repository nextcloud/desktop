/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "livecapture.h"

#include "accountmanager.h"
#include "accountstate.h"
#include "activity/activitylistmodel.h"
#include "application.h"
#include "assistant/assistantcontroller.h"
#include "catalogue.h"
#include "ignorelisteditor.h"
#include "liveprofile.h"
#include "owncloudgui.h"
#include "search/unifiedsearchresultslistmodel.h"
#include "settingsdialog.h"
#include "sharing/getsharesjob.h"
#include "sharing/sharingcontroller.h"
#include "sharing/unifiedshare.h"
#include "sharingsource.h"
#include "systray.h"
#include "tray/usermodel.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QWidget>
#include <functional>

namespace OCC::DocAssets
{
namespace
{
constexpr auto readinessIntervalMs = 40;
constexpr auto stableRenderTicks = 3;

bool waitUntil(const std::function<bool()> &condition, int timeoutMs)
{
    QElapsedTimer elapsed;
    elapsed.start();
    while (elapsed.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, readinessIntervalMs);
        if (condition()) {
            return true;
        }
        QThread::msleep(readinessIntervalMs);
    }
    return false;
}

bool imagesReady(QQuickItem *item)
{
    if (!item) {
        return false;
    }
    constexpr auto imageReady = 1;
    if (item->inherits("QQuickImage") && !item->property("source").toUrl().isEmpty() && item->property("status").toInt() != imageReady) {
        return false;
    }
    for (auto *child : item->childItems()) {
        if (!imagesReady(child)) {
            return false;
        }
    }
    return true;
}

bool saveImage(const QImage &image, const QString &path, QString *error)
{
    if (image.isNull() || !image.save(path, "PNG") || QImageReader(path, "PNG").read().isNull()) {
        *error = QStringLiteral("Could not save and read back %1").arg(path);
        return false;
    }
    return true;
}

bool saveWidget(QWidget *widget, const QString &path, int timeoutMs, QString *error)
{
    auto stableTicks = 0;
    auto previousSize = QSize{};
    const auto ready = waitUntil(
        [&] {
            if (!widget || !widget->isVisible()) {
                return false;
            }
            if (widget->size() != previousSize) {
                previousSize = widget->size();
                stableTicks = 0;
            }
            return ++stableTicks >= stableRenderTicks;
        },
        timeoutMs);
    if (!ready) {
        *error = QStringLiteral("Timed out waiting for the production widget");
        return false;
    }
    return saveImage(widget->grab().toImage(), path, error);
}

bool saveQuickWindow(QQuickWindow *window, const QString &path, int timeoutMs, QString *error)
{
    auto stableTicks = 0;
    auto previousSize = QSize{};
    const auto ready = waitUntil(
        [&] {
            if (!window || !window->isVisible() || !window->isExposed() || !window->isSceneGraphInitialized() || !imagesReady(window->contentItem())) {
                stableTicks = 0;
                return false;
            }
            window->update();
            if (window->size() != previousSize) {
                previousSize = window->size();
                stableTicks = 0;
            }
            return ++stableTicks >= stableRenderTicks;
        },
        timeoutMs);
    if (!ready) {
        *error = QStringLiteral("Timed out waiting for the production QML window");
        return false;
    }
    return saveImage(window->grabWindow(), path, error);
}

template<typename T>
T *visibleTopLevelWidget()
{
    for (auto *widget : QApplication::topLevelWidgets()) {
        if (auto *match = qobject_cast<T *>(widget); match && match->isVisible()) {
            return match;
        }
    }
    return nullptr;
}

QQuickWindow *visibleQuickWindow(const std::function<bool(QQuickWindow *)> &matches)
{
    for (auto *window : QGuiApplication::topLevelWindows()) {
        if (auto *quickWindow = qobject_cast<QQuickWindow *>(window); quickWindow && quickWindow->isVisible() && matches(quickWindow)) {
            return quickWindow;
        }
    }
    return nullptr;
}

bool selectSettingsPage(SettingsDialog *dialog, const QString &page)
{
    for (auto *action : dialog->findChildren<QAction *>()) {
        if (action->text().remove(QLatin1Char('&')) == page) {
            action->trigger();
            return true;
        }
    }
    return false;
}

bool prepareSettings(Application &application, AccountState *accountState, const QString &scenario, QWidget **target, int timeoutMs, QString *error)
{
    application.gui()->slotShowSettings();
    auto *dialog = visibleTopLevelWidget<SettingsDialog>();
    if (!dialog
        && !waitUntil(
            [&] {
                return (dialog = visibleTopLevelWidget<SettingsDialog>());
            },
            timeoutMs)) {
        *error = QStringLiteral("The production Settings dialog did not open");
        return false;
    }
    if (!waitUntil(
            [&] {
                return dialog->currentPage() != nullptr;
            },
            timeoutMs)) {
        *error = QStringLiteral("The production Settings dialog did not select its initial page");
        return false;
    }
    if (scenario == QStringLiteral("settings-account-classic")) {
        dialog->showAccount(accountState);
        if (!waitUntil(
                [&] {
                    return dialog->currentPage() && dialog->currentPage()->objectName().startsWith(QStringLiteral("accountSettings_"));
                },
                timeoutMs)) {
            *error = QStringLiteral("The production account Settings page did not open");
            return false;
        }
        *target = dialog;
        return true;
    }
    const auto page = scenario == QStringLiteral("settings-general") ? QStringLiteral("General")
        : scenario == QStringLiteral("settings-advanced")            ? QStringLiteral("Advanced")
        : scenario == QStringLiteral("settings-info")                ? QStringLiteral("Info")
        : scenario == QStringLiteral("settings-ignored-files")       ? QStringLiteral("Advanced")
                                                                     : QString{};
    if (page.isEmpty() || !selectSettingsPage(dialog, page)) {
        *error = QStringLiteral("Could not select the requested Settings page");
        return false;
    }
    if (!waitUntil(
            [&] {
                return dialog->currentPage() && dialog->currentPage()->objectName() == QStringLiteral("settingsPage_") + page;
            },
            timeoutMs)) {
        *error = QStringLiteral("The production Settings page did not become active");
        return false;
    }
    if (scenario != QStringLiteral("settings-ignored-files")) {
        *target = dialog;
        return true;
    }
    auto *button = dialog->findChild<QAbstractButton *>(QStringLiteral("ignoredFilesButton"));
    if (!button) {
        *error = QStringLiteral("The production Ignored Files button is unavailable");
        return false;
    }
    button->click();
    auto *editor = visibleTopLevelWidget<IgnoreListEditor>();
    if (!editor
        && !waitUntil(
            [&] {
                return (editor = visibleTopLevelWidget<IgnoreListEditor>());
            },
            timeoutMs)) {
        *error = QStringLiteral("The production Ignored Files dialog did not open");
        return false;
    }
    *target = editor;
    return true;
}

bool prepareSharing(const AccountPtr &account, const QString &scenario, QQuickWindow **target, int timeoutMs, QString *error)
{
    if (!account->capabilities().unifiedSharingAvailable()) {
        *error = QStringLiteral("This server does not support the unified Sharing dialog.");
        return false;
    }
    QVariantMap properties;
    {
        QObject requestScope;
        auto completed = false;
        const auto job = new Gui::Sharing::GetSharesJob(account);
        job->setParent(&requestScope);
        QObject::connect(job, &Gui::Sharing::GetSharesJob::sharesFetched, &requestScope, [&](const QJsonDocument &json) {
            properties = sharingSourceProperties(json, error);
            completed = true;
        });
        QObject::connect(job, &OcsJob::ocsError, &requestScope, [&](int, const QString &message) {
            *error = QStringLiteral("Could not list existing server shares: %1").arg(message);
            completed = true;
        });
        job->start();
        if (!waitUntil(
                [&] {
                    return completed;
                },
                timeoutMs)) {
            *error = QStringLiteral("Timed out listing existing server shares.");
            return false;
        }
    }
    if (properties.isEmpty()) {
        return false;
    }
    properties.insert(QStringLiteral("account"), QVariant::fromValue(account));
    // Use the same component and tray engine as Systray, supplying the server's
    // file ID and display name rather than resolving a local sync journal.
    auto *engine = Systray::instance()->trayEngine();
    if (!engine) {
        *error = QStringLiteral("The production Sharing QML engine is unavailable.");
        return false;
    }
    QQmlComponent component(engine, QStringLiteral("com.nextcloud.desktopclient.sharing"), QStringLiteral("ShareDialog"));
    auto *object = component.createWithInitialProperties(properties);
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        delete object;
        *error = QStringLiteral("Could not open the production Sharing dialog: %1").arg(component.errorString());
        return false;
    }
    window->QObject::setParent(engine);
    window->show();
    auto *controller = window->findChild<Gui::Sharing::SharingController *>(QStringLiteral("sharingController"));
    if (!controller
        || !waitUntil(
            [&] {
                return !controller->shares().isEmpty();
            },
            timeoutMs)) {
        *error = QStringLiteral("The server share was found, but the production Sharing dialog did not finish loading it.");
        return false;
    }
    if (scenario != QStringLiteral("sharing-overview")) {
        auto *share = controller->shares().constFirst();
        window->setProperty("selectedShare", QVariant::fromValue(share));
        window->setProperty("hasSelectedShare", true);
        if (scenario == QStringLiteral("sharing-advanced-settings")) {
            window->setProperty("advancedSettingsVisible", true);
        }
    }
    *target = window;
    return true;
}
}

bool captureLiveScenario(Application &application,
                         const LiveProfile &profile,
                         const QString &scenario,
                         const QString &stagingDirectory,
                         int timeoutMs,
                         QString *error,
                         bool *skipped)
{
    error->clear();
    *skipped = false;
    const auto filename = captureFilename(scenario);
    if (filename.isEmpty() || !scenarioUsesLiveProfile(scenario)) {
        *error = QStringLiteral("Unknown live-account scenario: %1").arg(scenario);
        return false;
    }
    const auto accounts = AccountManager::instance()->accounts();
    if (accounts.size() != 1) {
        *error = QStringLiteral("The dedicated capture profile must contain exactly one account");
        return false;
    }
    auto *accountState = accounts.constFirst().data();
    if (!accountMatchesProfile(accountState->account().data(), profile, error)) {
        return false;
    }
    if (!waitUntil(
            [&] {
                return accountState->state() == AccountState::Connected;
            },
            timeoutMs)) {
        *error = QStringLiteral("The dedicated test account did not become connected");
        return false;
    }
    const auto userIndex = UserModel::instance()->findUserIdForAccount(accountState);
    if (userIndex < 0) {
        *error = QStringLiteral("The connected account is unavailable in the production user model");
        return false;
    }
    const auto output = QDir(stagingDirectory).filePath(filename);
    if (scenario.startsWith(QStringLiteral("settings-"))) {
        QWidget *target = nullptr;
        return prepareSettings(application, accountState, scenario, &target, timeoutMs, error) && saveWidget(target, output, timeoutMs, error);
    }
    QQuickWindow *target = nullptr;
    if (scenario == QStringLiteral("activities")) {
        Systray::instance()->showActivitiesWindow(userIndex);
        if (waitUntil(
                [&] {
                    target = visibleQuickWindow([](QQuickWindow *window) {
                        return window->property("headline").toString() == QStringLiteral("Activities");
                    });
                    return target;
                },
                timeoutMs)) {
            auto *model = target->property("activityModel").value<ActivityListModel *>();
            if (!model
                || !waitUntil(
                    [&] {
                        return model->rowCount() > 0;
                    },
                    timeoutMs)) {
                *error = QStringLiteral("The dedicated account returned no Activities");
                return false;
            }
        }
    } else if (scenario == QStringLiteral("search-results")) {
        Systray::instance()->showSearchWindow(userIndex);
        if (waitUntil(
                [&] {
                    target = visibleQuickWindow([](QQuickWindow *window) {
                        return window->property("headline").toString() == QStringLiteral("Search");
                    });
                    return target;
                },
                timeoutMs)) {
            auto *model = target->property("searchModel").value<UnifiedSearchResultsListModel *>();
            if (!model) {
                *error = QStringLiteral("The production Search model is unavailable");
                return false;
            }
            model->setSearchTerm(profile.searchTerm);
            if (!waitUntil(
                    [&] {
                        return model->providersReady() && !model->isSearchInProgress() && !model->waitingForSearchTermEditEnd()
                            && (model->rowCount() > 0 || !model->errorString().isEmpty());
                    },
                    timeoutMs)) {
                *error = model->errorString().isEmpty() ? QStringLiteral("The production Search did not settle") : model->errorString();
                return false;
            }
            if (model->rowCount() == 0) {
                *error = model->errorString().isEmpty() ? QStringLiteral("The dedicated account returned no Search results") : model->errorString();
                return false;
            }
        }
    } else if (scenario == QStringLiteral("user-status")) {
        Systray::instance()->showUserStatusWindow(userIndex);
        if (!waitUntil(
                [&] {
                    target = visibleQuickWindow([](QQuickWindow *window) {
                        return window->title() == QStringLiteral("Online status");
                    });
                    return target && target->property("statusLoaded").toBool();
                },
                timeoutMs)) {
            *error = QStringLiteral("The production User Status window did not finish loading");
            return false;
        }
    } else if (scenario == QStringLiteral("assistant-chat")) {
        const auto user = UserModel::instance()->user(userIndex);
        if (user && !user->isNcAssistantEnabled()) {
            *skipped = true;
            *error = QStringLiteral("Assistant is not available for the configured account.");
            return false;
        }
        Systray::instance()->showAssistantWindow(userIndex);
        if (!waitUntil(
                [&] {
                    target = visibleQuickWindow([](QQuickWindow *window) {
                        return window->objectName() == QStringLiteral("assistantWindow");
                    });
                    if (!target) {
                        return false;
                    }
                    const auto controller = target->property("assistantController").value<AssistantController *>();
                    return controller && controller->property("assistantEnabled").toBool() && !controller->property("requestInProgress").toBool();
                },
                timeoutMs)) {
            *error = QStringLiteral("The production Assistant window did not finish loading");
            return false;
        }
    } else if (scenario.startsWith(QStringLiteral("sharing-"))) {
        if (!prepareSharing(accountState->account(), scenario, &target, timeoutMs, error)) {
            return false;
        }
    }
    if (!target) {
        *error = QStringLiteral("The production window for %1 did not open").arg(scenario);
        return false;
    }
    return saveQuickWindow(target, output, timeoutMs, error);
}
}
