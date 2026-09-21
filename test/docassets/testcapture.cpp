/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "accountmanager.h"
#include "accountsettings.h"
#include "accountstate.h"
#include "activity/syncstatussummary.h"
#include "arguments.h"
#include "capture.h"
#include "captureguard.h"
#include "capturescene.h"
#include "cocoainitializer.h"
#include "configfile.h"
#include "exporter.h"
#include "fixtureaccount.h"
#include "fixtureactivitylistmodel.h"
#include "fixturefolderstatusmodel.h"
#include "fixtureimageprovider.h"
#include "fixturenetworkaccessmanager.h"
#include "fixturereply.h"
#include "fixtureuserstatusmodel.h"
#include "folderstatusdelegate.h"
#include "generalsettings.h"
#include "iconexport.h"
#include "ignorelisteditor.h"
#include "logger.h"
#include "offlinenetworkfactory.h"
#include "outputdirectory.h"
#include "search/unifiedsearchresultslistmodel.h"
#include "searchfixtures.h"
#include "settingsdialog.h"
#include "settingsfixtures.h"
#include "setup.h"
#include "sharingfixtures.h"
#include <QAbstractButton>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSet>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeView>
#include <QUrlQuery>
#include <QtTest>
#include <memory>

using namespace OCC;
using namespace OCC::DocAssets;

class TestCapture : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void nativeStyleWarningDoesNotAbort()
    {
        QQmlError warning;
        warning.setMessageType(QtWarningMsg);
        warning.setUrl(QUrl(QStringLiteral("qrc:/qml/src/gui/wizard/qml/WizardTextField.qml")));
        warning.setDescription(
            QStringLiteral("QML QQuickRectangle: The current style does not support customization of this control "
                           "(property: \"background\" item: QQuickRectangle(0x1234, parent=0x0, geometry=0,0 0x0))."));
        QString error;
        recordQmlWarnings({warning, warning}, &error);
        QVERIFY(error.isEmpty());

        auto emojiWarning = warning;
        emojiWarning.setUrl(QUrl(QStringLiteral("qrc:/qml/src/gui/EmojiPicker.qml")));
        recordQmlWarnings({emojiWarning}, &error);
        QVERIFY(error.isEmpty());

        auto critical = warning;
        critical.setMessageType(QtCriticalMsg);
        recordQmlWarnings({critical}, &error);
        QCOMPARE(error, critical.toString());
        recordQmlWarnings({warning}, &error);
        QCOMPARE(error, critical.toString());

        error.clear();
        auto otherComponent = warning;
        otherComponent.setUrl(QUrl(QStringLiteral("qrc:/unexpected.qml")));
        recordQmlWarnings({otherComponent}, &error);
        QCOMPARE(error, otherComponent.toString());

        error.clear();
        auto otherProperty = warning;
        otherProperty.setDescription(warning.description().replace(QStringLiteral("background"), QStringLiteral("contentItem")));
        recordQmlWarnings({otherProperty}, &error);
        QCOMPARE(error, otherProperty.toString());

        error.clear();
        auto bindingFailure = warning;
        bindingFailure.setDescription(QStringLiteral("ReferenceError: missingController is not defined"));
        recordQmlWarnings({warning, bindingFailure}, &error);
        QCOMPARE(error, bindingFailure.toString());
    }

    void xcodeOptionsPreserveCaptureArguments()
    {
        QString error;
        QCOMPARE(
            normalizedCaptureArguments({"capture", "-ApplePersistenceIgnoreState", "YES", "--output", "/tmp/new folder", "-NSDocumentRevisionsDebugMode", "NO"},
                                       &error),
            (QStringList{"capture", "--output", "/tmp/new folder"}));
        QVERIFY(error.isEmpty());
        QCOMPARE(normalizedCaptureArguments({"capture", "--unknown"}, &error), (QStringList{"capture", "--unknown"}));
        QVERIFY(normalizedCaptureArguments({"capture", "-ApplePersistenceIgnoreState"}, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(normalizedCaptureArguments({"capture", "-NSDocumentRevisionsDebugMode", "invalid"}, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void defaultOutputUsesRealDownloadsDespiteTestMode()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        const auto first = downloadsExportDirectory();
        QCOMPARE(QFileInfo(first).absolutePath(), QDir::home().filePath(QStringLiteral("Downloads")));
        QVERIFY(first != downloadsExportDirectory());
        QVERIFY(newExportDirectory(QStringLiteral("relative")).isEmpty());
        QVERIFY(newExportDirectory({}).isEmpty());
    }

    void outputValidationPreservesExistingFiles()
    {
        QTemporaryDir directory;
        QFile marker(directory.filePath(QStringLiteral("keep.txt")));
        QVERIFY(marker.open(QIODevice::WriteOnly));
        marker.write("preserve");
        marker.close();
        QString error;
        QVERIFY(!exportCatalogue(QStringLiteral(DOC_ASSETS_EXECUTABLE), directory.path(), {scenarioId}, &error));
        QVERIFY(error.contains(QStringLiteral("new absolute folder")));
        QVERIFY(!exportCatalogue(QStringLiteral(DOC_ASSETS_EXECUTABLE), QStringLiteral("relative"), {scenarioId}, &error));
        QVERIFY(!exportCatalogue(QStringLiteral(DOC_ASSETS_EXECUTABLE), directory.filePath(QStringLiteral("missing/child")), {scenarioId}, &error));
        QVERIFY(marker.open(QIODevice::ReadOnly));
        QCOMPARE(marker.readAll(), QByteArray("preserve"));
    }

    void networkRequestsFailLocally()
    {
        OfflineNetworkFactory factory;
        std::unique_ptr<QNetworkAccessManager> manager(factory.create(nullptr));
        QVERIFY(!factory.requestAttempted());
        auto *reply = manager->get(QNetworkRequest(QUrl(QStringLiteral("https://cloud.example.com/should-not-connect"))));
        QSignalSpy finished(reply, &QNetworkReply::finished);
        QTRY_VERIFY(reply->isFinished());
        QTRY_COMPARE(finished.count(), 1);
        QVERIFY(reply->error() != QNetworkReply::NoError);
        QCOMPARE(reply->url().scheme(), QStringLiteral("qrc"));
        QVERIFY(factory.requestAttempted());
        reply->deleteLater();
    }

    void missingWindowFailsWithDiagnostics()
    {
        QTemporaryDir directory;
        QString error;
        QVERIFY(!captureScenario(scenarioId, directory.path(), captureTimeoutMs, &error, QUrl(QStringLiteral("qrc:/missing-window.qml"))));
        QVERIFY(error.contains(QStringLiteral("Could not create production window")));
        QVERIFY(QDir(directory.path()).entryList(QDir::Files).isEmpty());
    }

    void timeoutDoesNotPublishOutput()
    {
        QTemporaryDir directory;
        const auto destination = directory.filePath(QStringLiteral("export"));
        QString error;
        QVERIFY(!captureScenario(scenarioId, directory.path(), 0, &error));
        QVERIFY2(error.contains(QStringLiteral("Timed out")), qPrintable(error));
        QVERIFY(!QFileInfo::exists(destination));
        QVERIFY(QDir(directory.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }

    void closedWindowCancelsExport()
    {
        QTemporaryDir directory;
        QTimer closeWindow;
        connect(&closeWindow, &QTimer::timeout, &closeWindow, [&] {
            for (auto *window : QGuiApplication::topLevelWindows()) {
                if (qobject_cast<QQuickWindow *>(window) && window->isVisible()) {
                    closeWindow.stop();
                    window->close();
                }
            }
        });
        closeWindow.start(1);
        QString error;
        QVERIFY(!captureScenario(scenarioId, directory.path(), captureTimeoutMs, &error));
        QVERIFY2(error.contains(QStringLiteral("cancelled")), qPrintable(error));
        QVERIFY(QDir(directory.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }

    void fixtureReplyCompletesAndCancelsOnce()
    {
        const QNetworkRequest request(QUrl(QStringLiteral("https://cloud.example.com/fixture")));
        FixtureReply reply(QNetworkAccessManager::GetOperation, request, QByteArray("payload"), "text/plain", nullptr);
        QSignalSpy finished(&reply, &QNetworkReply::finished);
        QTRY_VERIFY(reply.isFinished());
        QCOMPARE(reply.error(), QNetworkReply::NoError);
        QCOMPARE(reply.readAll(), QByteArray("payload"));
        reply.abort();
        QCOMPARE(finished.count(), 1);

        FixtureReply cancelled(QNetworkAccessManager::GetOperation, request, QByteArray("payload"), "text/plain", nullptr);
        QSignalSpy cancelledFinished(&cancelled, &QNetworkReply::finished);
        cancelled.abort();
        cancelled.abort();
        QCoreApplication::processEvents();
        QCOMPARE(cancelled.error(), QNetworkReply::OperationCanceledError);
        QCOMPARE(cancelledFinished.count(), 1);

        FixtureReply duringRead(QNetworkAccessManager::GetOperation, request, QByteArray("payload"), "text/plain", nullptr);
        QSignalSpy duringReadFinished(&duringRead, &QNetworkReply::finished);
        connect(&duringRead, &QNetworkReply::readyRead, &duringRead, &QNetworkReply::abort);
        QTRY_VERIFY(duringRead.isFinished());
        QCOMPARE(duringRead.error(), QNetworkReply::OperationCanceledError);
        QCOMPARE(duringReadFinished.count(), 1);
    }

    void fixtureImagesUseProductionResources()
    {
        FixtureImageProvider provider;
        std::unique_ptr<QQuickImageResponse> avatar(
            provider.requestImageResponse(QStringLiteral("https://cloud.example.com/index.php/avatar/jamie/64"), QSize(32, 32)));
        std::unique_ptr<QQuickTextureFactory> texture(avatar->textureFactory());
        QVERIFY(texture);
        QVERIFY(!texture->image().isNull());
        QVERIFY(!provider.rejectedRequest());
        std::unique_ptr<QQuickImageResponse> rejected(provider.requestImageResponse(QStringLiteral("https://example.invalid/avatar"), QSize(32, 32)));
        QVERIFY(provider.rejectedRequest());
    }

    void missingFixtureFailsWithoutNetworkFallback()
    {
        FixtureNetworkAccessManager manager;
        auto *reply = manager.get(QNetworkRequest(QUrl(QStringLiteral("https://example.invalid/not-a-fixture"))));
        QTRY_VERIFY(reply->isFinished());
        QCOMPARE(reply->error(), QNetworkReply::ContentNotFoundError);
        QCOMPARE(manager.missingFixtures.size(), 1);
        QCOMPARE(manager.requests, manager.missingFixtures);
        reply->deleteLater();
    }

    void customGetConsumesFixture()
    {
        FixtureNetworkAccessManager manager;
        const auto url = QUrl(QStringLiteral("https://cloud.example.com/ocs/v2.php/apps/sharing/api/v1/shares?format=json"));
        const auto payload = QByteArrayLiteral(R"({"ocs":{"data":[]}})");
        manager.getResponses.insert(url, payload);

        auto *reply = manager.sendCustomRequest(QNetworkRequest(url), QByteArrayLiteral("GET"));
        QTRY_VERIFY(reply->isFinished());
        QCOMPARE(reply->error(), QNetworkReply::NoError);
        QCOMPARE(reply->readAll(), payload);
        QVERIFY(manager.missingFixtures.isEmpty());
        reply->deleteLater();
    }

    void sceneDestroysModelsBeforeTheirAccountState()
    {
        QPointer<AccountState> state;
        QPointer<UnifiedSearchResultsListModel> model;
        auto destroyedModels = 0;
        auto accountAliveDuringModelDestruction = true;
        {
            CaptureScene scene;
            QVERIFY(prepareSearch(scene));
            state = scene.accountState.data();
            model = scene.findChild<UnifiedSearchResultsListModel *>();
            QVERIFY(model);
            new SyncStatusSummary(SyncResult::Problem, &scene);
            for (auto *child : scene.children()) {
                connect(child, &QObject::destroyed, this, [&] {
                    ++destroyedModels;
                    accountAliveDuringModelDestruction &= state && state->account() && state->account()->davUser() == QStringLiteral("alex");
                });
            }
        }
        QVERIFY(model.isNull());
        QVERIFY(state.isNull());
        QCOMPARE(destroyedModels, 2);
        QVERIFY(accountAliveDuringModelDestruction);
    }

    void realSearchConsumesFixturesAndFilters()
    {
        CaptureScene scene;
        QString error;
        QVERIFY(prepareSearch(scene));
        auto *model = scene.findChild<UnifiedSearchResultsListModel *>();
        QVERIFY(model);
        QTRY_VERIFY(scene.ready(&error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(model->rowCount() >= 5);
        QVERIFY(model->providersReady());
        model->setPersonFilter(QStringLiteral("jamie"), QStringLiteral("Jamie Rivera"));
        QVERIFY(model->setCustomDateRange(QStringLiteral("2026-09-01"), QStringLiteral("2026-09-17")));
        QTRY_VERIFY(scene.ready(&error));
        auto *network = static_cast<FixtureNetworkAccessManager *>(scene.account->networkAccessManager());
        QVERIFY(network->missingFixtures.isEmpty());
        auto filteredRequest = false;
        for (const auto &request : network->requests) {
            const QUrlQuery query{QUrl(request)};
            if (query.queryItemValue(QStringLiteral("person")) == QStringLiteral("jamie")) {
                QCOMPARE(query.queryItemValue(QStringLiteral("term")), QStringLiteral("Project"));
                if (query.hasQueryItem(QStringLiteral("since")) && query.hasQueryItem(QStringLiteral("until"))) {
                    filteredRequest = true;
                }
            }
        }
        QVERIFY(filteredRequest);
        QVERIFY(!model->activeFilters().isEmpty());
        QVERIFY(AccountManager::instance()->accounts().isEmpty());
    }

    void userStatusUsesInjectedProductionModel()
    {
        FixtureUserStatusModel model;
        QVERIFY(model.userStatusLoaded());
        QVERIFY(model.errorMessage().isEmpty());
        QCOMPARE(model.onlineStatus(), UserStatus::OnlineStatus::Online);
        QCOMPARE(model.userStatusMessage(), QStringLiteral("Reviewing the project plan"));
        QCOMPARE(model.predefinedStatuses().size(), 6);
        const auto statuses = model.predefinedStatuses();
        const QStringList messages{QStringLiteral("In a meeting"),
                                   QStringLiteral("Commuting"),
                                   QStringLiteral("Be right back"),
                                   QStringLiteral("Working remotely"),
                                   QStringLiteral("Out sick"),
                                   QStringLiteral("Vacationing")};
        for (auto index = 0; index < statuses.size(); ++index) {
            const auto status = statuses.at(index);
            QCOMPARE(status.message(), messages.at(index));
            QVERIFY(!status.icon().isEmpty());
        }
        QVERIFY(statuses.at(0).clearAt());
        QCOMPARE(statuses.at(0).clearAt()->_period, 3600);
        QVERIFY(statuses.at(3).clearAt());
        QCOMPARE(statuses.at(3).clearAt()->_endof, QStringLiteral("day"));
        QVERIFY(!statuses.at(5).clearAt());
        QVERIFY(model.busyStatusSupported());
        QVERIFY(AccountManager::instance()->accounts().isEmpty());
    }

    void generalSettingsUsesInjectedPlatformOperations()
    {
        auto enabled = false;
        auto writes = 0;
        auto services = settingsServices();
        services.autoStart = [&] {
            return enabled;
        };
        services.setAutoStart = [&](bool value) {
            enabled = value;
            ++writes;
        };
        GeneralSettings settings(nullptr, services);
        auto *checkbox = settings.findChild<QAbstractButton *>(QStringLiteral("autostartCheckBox"));
        QVERIFY(checkbox);
        QCOMPARE(writes, 0);
        checkbox->setChecked(true);
        QVERIFY(enabled);
        QCOMPARE(writes, 1);
        checkbox->setChecked(false);
        QVERIFY(!enabled);
        QCOMPARE(writes, 2);

        services.systemAutoStart = [] {
            return true;
        };
        GeneralSettings systemSettings(nullptr, services);
        auto *systemCheckbox = systemSettings.findChild<QAbstractButton *>(QStringLiteral("autostartCheckBox"));
        QVERIFY(systemCheckbox);
        QVERIFY(systemCheckbox->isChecked());
        QVERIFY(!systemCheckbox->isEnabled());
        QCOMPARE(writes, 2);
    }

    void settingsCapturesUseWholeWindow()
    {
        const QStringList pages{QStringLiteral("General"), QStringLiteral("Advanced"), QStringLiteral("Info")};
        for (const auto &page : pages) {
            CaptureScene scene;
            QString error;
            QVERIFY(prepareSettings(scene, QStringLiteral("settings-") + page.toLower(), &error));
            auto *dialog = qobject_cast<SettingsDialog *>(scene.widget.get());
            QVERIFY(dialog);
            // Allow the production initial-page selection before selecting this scenario.
            QTest::qWait(10);
            QVERIFY(scene.activate(nullptr, &error));
            QVERIFY(scene.settled(nullptr, &error));
            QCOMPARE(dialog->currentPage()->objectName(), QStringLiteral("settingsPage_") + page);
            for (const auto &other : pages) {
                QVERIFY(dialog->findChild<QWidget *>(QStringLiteral("settingsPage_") + other));
            }
            QVERIFY(AccountManager::instance()->accounts().isEmpty());
        }
        CaptureScene invalid;
        QString error;
        QVERIFY(!prepareSettings(invalid, QStringLiteral("settings-unknown"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!invalid.widget);
    }

    void classicAccountSettingsUsesSyntheticFolderAndQuota()
    {
        CaptureScene scene;
        QString error;
        QVERIFY(prepareSettings(scene, QStringLiteral("settings-account-classic"), &error));
        auto *dialog = qobject_cast<SettingsDialog *>(scene.widget.get());
        QVERIFY(dialog);
        QTest::qWait(10);
        QVERIFY(scene.activate(nullptr, &error));
        QVERIFY(scene.settled(nullptr, &error));
        auto *page = qobject_cast<AccountSettings *>(dialog->currentPage());
        QVERIFY(page);
        QCOMPARE(page->accountsState(), scene.accountState.data());
        auto *tree = page->findChild<QTreeView *>(QStringLiteral("_folderList"));
        QVERIFY(tree);
        QCOMPARE(tree->model()->rowCount(), 2);
        QCOMPARE(tree->model()->index(0, 0).data(FolderStatusDelegate::FolderPathRole).toString(), QStringLiteral("/Users/alex/Nextcloud"));
        QVERIFY(!tree->model()->canFetchMore(tree->model()->index(0, 0)));
        auto *label = page->findChild<QLabel *>(QStringLiteral("connectLabel"));
        QVERIFY(label);
        QVERIFY(label->text().contains(QStringLiteral("Alex Morgan")));
        QVERIFY(label->text().contains(QStringLiteral("12 GB")));
        page->slotAccountStateChanged();
        QVERIFY(AccountManager::instance()->accounts().isEmpty());
    }

    void ignoredFilesCaptureUsesProductionDialog()
    {
        CaptureScene scene;
        QString error;
        QVERIFY(prepareSettings(scene, QStringLiteral("settings-ignored-files"), &error));
        auto *dialog = qobject_cast<IgnoreListEditor *>(scene.widget.get());
        QVERIFY(dialog);
        auto *table = dialog->findChild<QTableWidget *>(QStringLiteral("tableWidget"));
        QVERIFY(table);
        QCOMPARE(table->rowCount(), 5);
        QCOMPARE(table->item(3, 0)->text(), QStringLiteral("*.tmp"));
        QCOMPARE(table->item(4, 0)->text(), QStringLiteral("build/"));
        QVERIFY(AccountManager::instance()->accounts().isEmpty());
    }

    void sharingCapturesUseProductionModuleAndLocalFixtures()
    {
        const auto scenarios = QStringList{
            QStringLiteral("sharing-overview"),
            QStringLiteral("sharing-details"),
            QStringLiteral("sharing-advanced-settings"),
        };
        for (const auto &scenario : scenarios) {
            CaptureScene scene;
            QString error;
            QVERIFY2(prepareSharing(scene, scenario, &error), qPrintable(error));
            QCOMPARE(scene.module, QStringLiteral("com.nextcloud.desktopclient.sharing"));
            QCOMPARE(scene.type, QStringLiteral("ShareDialog"));
            QCOMPARE(scene.properties.value(QStringLiteral("shortLocalPath")).toString(), QStringLiteral("Project plan.pdf"));
            QCOMPARE(scene.properties.value(QStringLiteral("fileId")).toString(), QStringLiteral("42"));
            auto *network = static_cast<FixtureNetworkAccessManager *>(scene.account->networkAccessManager());
            QCOMPARE(network->getResponses.size(), 1);
            auto parseError = QJsonParseError{};
            const auto response = QJsonDocument::fromJson(network->getResponses.constBegin().value(), &parseError);
            QCOMPARE(parseError.error, QJsonParseError::NoError);
            const auto shares = response.object().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toArray();
            QCOMPARE(shares.size(), 2);
            QCOMPARE(shares.at(0).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("share-team"));
            QCOMPARE(shares.at(1).toObject().value(QStringLiteral("id")).toString(), QStringLiteral("share-public"));
            QVERIFY(AccountManager::instance()->accounts().isEmpty());
        }

        CaptureScene invalid;
        QString error;
        QVERIFY(!prepareSharing(invalid, QStringLiteral("sharing-unknown"), &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(!invalid.account);
    }

    void activitiesUseUnregisteredAccountAndProductionRoles()
    {
        CaptureScene scene;
        prepareAccount(scene);
        FixtureActivityListModel model(scene.accountState, nullptr);
        QCOMPARE(model.rowCount(), 3);
        QVERIFY(model.hasSyncConflicts());
        QVERIFY(!model.canFetchMore({}));
        QCOMPARE(model.data(model.index(0), ActivityListModel::ActionTextRole).toString(), QStringLiteral("Project plan.pdf has a conflict"));
        QVERIFY(model.data(model.index(0), ActivityListModel::PathRole).toString().isEmpty());
        QVERIFY(!model.data(model.index(0), ActivityListModel::ActivityIntegrationRole).toBool());
        QVERIFY(AccountManager::instance()->accounts().isEmpty());

        SyncStatusSummary summary(SyncResult::Problem);
        QVERIFY(!summary.syncing());
        QCOMPARE(summary.syncIcon(), Theme::instance()->warning());
        SyncStatusSummary running(SyncResult::SyncRunning);
        QVERIFY(running.syncing());
        SyncStatusSummary success(SyncResult::Success);
        QVERIFY(!success.syncing());
        QVERIFY(success.syncStatusDetailString().isEmpty());
        QCOMPARE(success.syncIcon(), Theme::instance()->ok());
    }

    void invalidCatalogueNeverPublishes()
    {
        QTemporaryDir directory;
        QString error;
        const auto output = directory.filePath(QStringLiteral("export"));
        const auto executable = QStringLiteral(DOC_ASSETS_EXECUTABLE);
        QVERIFY(!exportCatalogue(executable, output, {}, &error));
        QVERIFY(!exportCatalogue(executable, output, {scenarioId, scenarioId}, &error));
        QVERIFY(error.contains(QStringLiteral("duplicate")));
        QVERIFY(!exportCatalogue(executable, output, {QStringLiteral("unknown")}, &error));
        QVERIFY(error.contains(QStringLiteral("Unknown")));
        QVERIFY(!exportCatalogue(directory.filePath(QStringLiteral("missing-worker")), output, {scenarioId}, &error));
        QVERIFY(error.contains(QStringLiteral("could not start")));
        QVERIFY(!exportCatalogue(executable, output, {scenarioId}, &error, 0));
        QVERIFY2(error.contains(QStringLiteral("timed out")), qPrintable(error));
        QVERIFY(QDir(directory.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }

    void failedWorkersDoNotPreventLaterCaptures()
    {
        QTemporaryDir directory;
        const auto output = directory.filePath(QStringLiteral("export"));
        QString error;
        QVERIFY(!exportCatalogue(QCoreApplication::applicationFilePath(),
                                 output,
                                 {scenarioId, QStringLiteral("search-results"), QStringLiteral("settings-general"), QStringLiteral("assistant-chat")},
                                 &error));
        QVERIFY(error.contains(QStringLiteral("deliberate worker failure")));
        QVERIFY(error.contains(QStringLiteral("search-results")));
        QVERIFY(error.contains(QStringLiteral("settings-general")));
        QVERIFY(!QFileInfo::exists(output));
        const auto partials = QDir(directory.path()).entryList({QStringLiteral("export-incomplete-*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        QCOMPARE(partials.size(), 1);
        const QDir partial(directory.filePath(partials.first()));
        QCOMPARE(partial.entryList(QDir::Files), (QStringList{QStringLiteral("assistant-chat.png"), screenshotFilename}));
        QVERIFY(!QImageReader(partial.filePath(QStringLiteral("assistant-chat.png")), "PNG").read().isNull());
        QVERIFY(!QImageReader(partial.filePath(screenshotFilename), "PNG").read().isNull());
        QVERIFY(error.contains(partial.absolutePath()));
    }

    void productionIconsAreRenderedAsPng()
    {
        QTemporaryDir directory;
        QString error;
        const auto sources = statusIconSources();
        QCOMPARE(sources.size(), 18);
        QCOMPARE(sources.value(QStringLiteral("icon-status-ok")), QStringLiteral(":/client/theme/colored/ok.svg"));
        QCOMPARE(sources.value(QStringLiteral("icon-tray-ok")), QStringLiteral(":/client/theme/black/state-ok.svg"));
        QCOMPARE(sources.value(QStringLiteral("icon-tray-colored-ok")), QStringLiteral(":/client/theme/colored/state-ok.svg"));
        QVERIFY2(exportIcons(directory.path(), sources, &error), qPrintable(error));
        const auto names = iconFilenames(sources);
        QCOMPARE(QSet<QString>(names.begin(), names.end()).size(), names.size());
        for (auto it = sources.cbegin(); it != sources.cend(); ++it) {
            const auto filename = it.key() + QStringLiteral(".png");
            QVERIFY(names.contains(filename));
            const auto image = QImageReader(directory.filePath(filename), "PNG").read();
            QVERIFY2(!image.isNull(), qPrintable(filename));
            QCOMPARE(image.size(), QSize(256, 256));
            QVERIFY(image.hasAlphaChannel());
        }
        const auto statusImage = QImageReader(directory.filePath(QStringLiteral("icon-status-ok.png")), "PNG").read();
        QCOMPARE(statusImage.pixelColor(0, 0).alpha(), 0);
        QVERIFY(statusImage.pixelColor(128, 128).alpha() > 0);
        QTemporaryDir missing;
        QVERIFY(!exportIcons(missing.path(), {{QStringLiteral("missing"), QStringLiteral(":/missing.svg")}}, &error));
        QVERIFY(error.contains(QStringLiteral("missing")));
        QVERIFY(QDir(missing.path()).entryList(QDir::Files).isEmpty());
        QVERIFY(!exportIcons(missing.filePath(QStringLiteral("absent")), sources, &error));
        QVERIFY(!error.isEmpty());
    }

    void realCatalogueProducesCompleteAssetsInFreshWorkers()
    {
        QTemporaryDir directory;
        QString error;
        const auto output = directory.filePath(QStringLiteral("export"));
        const auto scenarios = captureScenarios();
        const QSet<QString> unique(scenarios.begin(), scenarios.end());
        QCOMPARE(unique.size(), scenarios.size());
        QVERIFY2(exportCatalogue(QStringLiteral(DOC_ASSETS_EXECUTABLE), output, scenarios, &error, captureTimeoutMs + workerStartupTimeoutMs, true),
                 qPrintable(error));
        auto filenames = QStringList{};
        for (const auto &scenario : scenarios) {
            const auto filename = captureFilename(scenario);
            filenames.append(filename);
            const auto image = QImageReader(QDir(output).filePath(filename), "PNG").read();
            QVERIFY2(!image.isNull(), qPrintable(scenario));
            QVERIFY(image.width() > 0 && image.height() > 0);
        }
        filenames.append(iconFilenames(statusIconSources()));
        filenames.sort();
        QCOMPARE(QDir(output).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name), filenames);
        const auto repeated = directory.filePath(QStringLiteral("repeat"));
        QVERIFY2(exportCatalogue(QStringLiteral(DOC_ASSETS_EXECUTABLE), repeated, {scenarioId}, &error), qPrintable(error));
        QCOMPARE(QImageReader(QDir(output).filePath(screenshotFilename)).size(), QImageReader(QDir(repeated).filePath(screenshotFilename)).size());
        QVERIFY(AccountManager::instance()->accounts().isEmpty());
    }
};

int main(int argc, char **argv)
{
    Q_INIT_RESOURCE(resources);
    Q_INIT_RESOURCE(theme);
    Q_INIT_RESOURCE(assistant);
    OCC::Mac::CocoaInitializer cocoa;
    QTemporaryDir configuration;
    if (!configuration.isValid() || !prepareEnvironment(configuration.path())) {
        return 1;
    }
    QApplication app(argc, argv);
    if (app.arguments().size() == 4 && app.arguments().at(1) == QStringLiteral("--capture-worker")) {
        Logger::instance()->setLogFile(QStringLiteral("-"));
        Logger::instance()->setLogFlush(true);
        if (app.arguments().at(2) != scenarioId && app.arguments().at(2) != QStringLiteral("assistant-chat")) {
            qCritical("deliberate worker failure");
            return 1;
        }
        QImage image(16, 16, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        return image.save(QDir(app.arguments().at(3)).filePath(captureFilename(app.arguments().at(2))), "PNG") ? 0 : 1;
    }
    app.setQuitOnLastWindowClosed(false);
    app.setLayoutDirection(Qt::LeftToRight);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);
    CaptureGuard guard;
    app.installEventFilter(&guard);
    QDesktopServices::setUrlHandler(QStringLiteral("https"), &guard, "consumeUrl");
    QDesktopServices::setUrlHandler(QStringLiteral("http"), &guard, "consumeUrl");
    registerWizardTypes();
    TestCapture test;
    return QTest::qExec(&test, argc, argv);
}

#include "testcapture.moc"
