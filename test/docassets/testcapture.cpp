/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "account.h"
#include "arguments.h"
#include "capture.h"
#include "catalogue.h"
#include "cocoainitializer.h"
#include "exporter.h"
#include "iconexport.h"
#include "liveprofile.h"
#include "setup.h"
#include "sharing/sharingconstants.h"
#include "sharingsource.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

using namespace OCC;
using namespace OCC::DocAssets;

class TestCapture : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void unsupportedAssistantDoesNotFailExport()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto executable = directory.filePath(QStringLiteral("skip-worker"));
        QFile worker(executable);
        QVERIFY(worker.open(QIODevice::WriteOnly));
        const auto script = QByteArray("#!/bin/sh\nexit ") + QByteArray::number(captureSkippedExitCode) + '\n';
        QCOMPARE(worker.write(script), script.size());
        worker.close();
        QVERIFY(worker.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QString error;
        const auto output = directory.filePath(QStringLiteral("export"));
        QVERIFY2(exportCatalogue(executable, output, {QStringLiteral("assistant-chat")}, &error, workerStartupTimeoutMs, true), qPrintable(error));
        QVERIFY(!QFileInfo::exists(QDir(output).filePath(QStringLiteral("assistant-chat.png"))));
        QCOMPARE(QDir(output).entryList({QStringLiteral("*.png")}, QDir::Files).size(), statusIconSources().size());
        QVERIFY(!exportCatalogue(executable, directory.filePath(QStringLiteral("required")), {QStringLiteral("sharing-overview")}, &error));
        QVERIFY(error.contains(QStringLiteral("worker failed")));
    }

    void sharingSourceDoesNotRequireClassicSync()
    {
        const auto source = QJsonObject{
            {QStringLiteral("class"), QString(Gui::Sharing::SourceTypeClasses::node)},
            {QStringLiteral("value"), QStringLiteral("42")},
            {QStringLiteral("display_name"), QStringLiteral("Existing shared document.pdf")},
        };
        const auto share = QJsonObject{{QStringLiteral("id"), QStringLiteral("share-1")}, {QStringLiteral("sources"), QJsonArray{source}}};
        const auto response = QJsonDocument(QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{{QStringLiteral("data"), QJsonArray{share}}}},
        });
        QString error;
        const auto properties = sharingSourceProperties(response, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(properties.value(QStringLiteral("fileId")).toString(), QStringLiteral("42"));
        QCOMPARE(properties.value(QStringLiteral("shortLocalPath")).toString(), QStringLiteral("Existing shared document.pdf"));
        QVERIFY(!properties.contains(QStringLiteral("localPath")));
        QVERIFY(sharingSourceProperties(QJsonDocument(QJsonObject{}), &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("invalid")));
        auto malformedShare = share;
        auto malformedSource = source;
        malformedSource.remove(QStringLiteral("value"));
        malformedShare.insert(QStringLiteral("sources"), QJsonArray{malformedSource});
        const auto missingFileId = QJsonDocument(QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{{QStringLiteral("data"), QJsonArray{malformedShare}}}},
        });
        QVERIFY(sharingSourceProperties(missingFileId, &error).isEmpty());
        QVERIFY(!error.isEmpty());
        const auto emptyList = QJsonDocument(QJsonObject{
            {QStringLiteral("ocs"), QJsonObject{{QStringLiteral("data"), QJsonArray{}}}},
        });
        QVERIFY(sharingSourceProperties(emptyList, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void bundledRunnerInitializesNotifications()
    {
        QProcess worker;
        worker.start(QString::fromUtf8(DOC_ASSETS_EXECUTABLE), {QStringLiteral("--check-platform")});
        QVERIFY(worker.waitForStarted());
        QVERIFY(worker.waitForFinished());
        const auto diagnostics = worker.readAllStandardError();
        QVERIFY2(worker.exitStatus() == QProcess::NormalExit, diagnostics.constData());
        QVERIFY2(worker.exitCode() == 0, diagnostics.constData());
    }

    void nativeStyleWarningDoesNotAbort()
    {
        QQmlError warning;
        warning.setMessageType(QtWarningMsg);
        warning.setUrl(QUrl(QStringLiteral("qrc:/qml/src/gui/wizard/qml/WizardTextField.qml")));
        warning.setDescription(QStringLiteral("The current style does not support customization of this control (property: \"background\" item:"));
        QString error;
        recordQmlWarnings({warning}, &error);
        QVERIFY(error.isEmpty());

        warning.setUrl(QUrl(QStringLiteral("qrc:/unexpected.qml")));
        recordQmlWarnings({warning}, &error);
        QCOMPARE(error, warning.toString());
    }

    void nativeLauncherArgumentsAreRemoved()
    {
        QString error;
        QCOMPARE(normalizedCaptureArguments({QStringLiteral("capture"),
                                             QStringLiteral("-ApplePersistenceIgnoreState"),
                                             QStringLiteral("YES"),
                                             QStringLiteral("--output"),
                                             QStringLiteral("/tmp/new folder")},
                                            &error),
                 (QStringList{QStringLiteral("capture"), QStringLiteral("--output"), QStringLiteral("/tmp/new folder")}));
        QVERIFY(error.isEmpty());
    }

    void liveProfileReadsExistingClientConfiguration()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath(QStringLiteral("nextclouddev.cfg"));
        QString error;
        QVERIFY(!liveProfileFromConfig(path, &error));
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("Accounts/test/url"), QStringLiteral("https://cloud.example.com"));
        settings.setValue(QStringLiteral("Accounts/test/dav_user"), QStringLiteral("documentation"));
        settings.sync();
        const auto profile = liveProfileFromConfig(path, &error);
        QVERIFY2(profile.has_value(), qPrintable(error));
        QCOMPARE(profile->serverUrl, QUrl(QStringLiteral("https://cloud.example.com")));
        QCOMPARE(profile->user, QStringLiteral("documentation"));
        QCOMPARE(profile->directory, directory.path());

        settings.setValue(QStringLiteral("Accounts/test/url"), QStringLiteral("file:///tmp/test"));
        settings.sync();
        QVERIFY(!liveProfileFromConfig(path, &error));
        settings.setValue(QStringLiteral("Accounts/test/url"), QStringLiteral("https://cloud.example.com"));
        settings.setValue(QStringLiteral("Accounts/second/url"), QStringLiteral("https://other.example.com"));
        settings.sync();
        QVERIFY(!liveProfileFromConfig(path, &error));
        QVERIFY(error.contains(QStringLiteral("exactly one")));
    }

    void accountMustMatchDedicatedProfile()
    {
        const auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://cloud.example.com/nextcloud")));
        account->setDavUser(QStringLiteral("documentation"));
        const auto profile = LiveProfile{QStringLiteral("/tmp/profile"),
                                         QUrl(QStringLiteral("https://cloud.example.com/nextcloud/")),
                                         QStringLiteral("documentation"),
                                         QStringLiteral("Project"),
                                         {}};
        QString error;
        QVERIFY(accountMatchesProfile(account.data(), profile, &error));
        auto wrongServer = profile;
        wrongServer.serverUrl = QUrl(QStringLiteral("https://personal.example.com"));
        QVERIFY(!accountMatchesProfile(account.data(), wrongServer, &error));
        QVERIFY(error.contains(QStringLiteral("server mismatch")));
        auto wrongUser = profile;
        wrongUser.user = QStringLiteral("personal-user");
        QVERIFY(!accountMatchesProfile(account.data(), wrongUser, &error));
        QVERIFY(error.contains(QStringLiteral("user mismatch")));
    }

    void onlyWizardScenariosUseFixtures()
    {
        QVERIFY(!scenarioUsesLiveProfile(QStringLiteral("wizard-server")));
        QVERIFY(scenarioUsesLiveProfile(QStringLiteral("settings-general")));
        QVERIFY(scenarioUsesLiveProfile(QStringLiteral("search-results")));
    }

    void wizardCaptureUsesProductionWindow()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString error;
        QVERIFY2(captureScenario(QStringLiteral("wizard-server"), directory.path(), captureTimeoutMs, &error), qPrintable(error));
        const auto image = QImageReader(directory.filePath(QStringLiteral("wizard-server.png")), "PNG").read();
        QVERIFY(!image.isNull());
        QVERIFY(image.width() > 0);
        QVERIFY(image.height() > 0);
    }

    void productionIconsAreRenderedAsPng()
    {
        QTemporaryDir directory;
        QString error;
        const auto sources = statusIconSources();
        QCOMPARE(sources.size(), 18);
        QVERIFY2(exportIcons(directory.path(), sources, &error), qPrintable(error));
        const auto names = iconFilenames(sources);
        QCOMPARE(QSet<QString>(names.begin(), names.end()).size(), names.size());
        for (const auto &name : names) {
            const auto image = QImageReader(directory.filePath(name), "PNG").read();
            QVERIFY2(!image.isNull(), qPrintable(name));
            QCOMPARE(image.size(), QSize(256, 256));
        }
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
    QApplication application(argc, argv);
    application.setOrganizationName(QStringLiteral("NextcloudDocAssetsTest"));
    application.setApplicationName(QStringLiteral("DocAssetsCaptureTest"));
    application.setQuitOnLastWindowClosed(false);
    registerWizardTypes();
    TestCapture test;
    return QTest::qExec(&test, argc, argv);
}

#include "testcapture.moc"
