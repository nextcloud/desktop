/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/wizard/accountwizardcontroller.h"
#include "configfile.h"
#include "theme.h"

#include <QScopeGuard>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace OCC;

class TestAccountWizardController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void normalizesCommonServerUrlSuffixes()
    {
        QCOMPARE(AccountWizardController::normalizeServerUrlInput(QStringLiteral(" https://cloud.example/index.php ")),
                 QStringLiteral("https://cloud.example/"));
        QCOMPARE(AccountWizardController::normalizeServerUrlInput(QStringLiteral("https://cloud.example/remote.php/dav/"), QStringLiteral("remote.php/dav/")),
                 QStringLiteral("https://cloud.example/"));
    }

    void defaultsToHttpsWhenSchemeIsMissing()
    {
        QCOMPARE(AccountWizardController::normalizeServerUrlInput(QString()),
                 QString());
        QCOMPARE(AccountWizardController::normalizeServerUrlInput(QStringLiteral("cloud.example")),
                 QStringLiteral("https://cloud.example"));
        QCOMPARE(AccountWizardController::normalizeServerUrlInput(QStringLiteral("cloud.example:8080")),
                 QStringLiteral("https://cloud.example:8080"));
        QCOMPARE(AccountWizardController::normalizeServerUrlInput(QStringLiteral("http://cloud.example")),
                 QStringLiteral("http://cloud.example"));
    }

    void invalidServerUrlStaysOnServerStep()
    {
        QFETCH(QString, serverUrl);

        AccountWizardController controller;
        QSignalSpy errorSpy(&controller, &AccountWizardController::errorTextChanged);

        controller.setServerUrl(serverUrl);
        controller.submitServerUrl();

        QCOMPARE(controller.currentStep(), AccountWizardController::ServerStep);
        QVERIFY(!controller.errorText().isEmpty());
        QCOMPARE(errorSpy.count(), 1);
    }

    void invalidServerUrlStaysOnServerStep_data()
    {
        QTest::addColumn<QString>("serverUrl");

        QTest::newRow("empty") << QString();
        QTest::newRow("plain text with spaces") << QStringLiteral("not a url");
        QTest::newRow("missing host") << QStringLiteral("https://");
        QTest::newRow("missing host with path") << QStringLiteral("https:///remote.php/dav");
        QTest::newRow("invalid scheme marker") << QStringLiteral("://cloud.example");
    }

    void rejectsIncompleteManualProxyBeforeServerCheck()
    {
        AccountWizardController controller;
        QSignalSpy errorSpy(&controller, &AccountWizardController::errorTextChanged);

        controller.setServerUrl(QStringLiteral("https://cloud.example"));
        controller.setProxyMode(2);
        controller.submitServerUrl();

        QCOMPARE(controller.currentStep(), AccountWizardController::ServerStep);
        QCOMPARE(controller.errorText(), QStringLiteral("Proxy settings are incomplete."));
        QCOMPARE(errorSpy.count(), 1);
    }

    void validatesManualProxyConfiguration()
    {
        AccountWizardController controller;
        QSignalSpy proxySpy(&controller, &AccountWizardController::proxySettingsChanged);

        QVERIFY(controller.proxySettingsValid());
        QCOMPARE(controller.proxyMode(), 0);

        controller.setProxyMode(2);
        QCOMPARE(controller.proxyMode(), 2);
        QCOMPARE(controller.manualProxyType(), 0);
        QVERIFY(!controller.proxySettingsValid());

        controller.setProxyHost(QStringLiteral("proxy.example"));
        QVERIFY(controller.proxySettingsValid());

        controller.setProxyAuthenticationRequired(true);
        QVERIFY(!controller.proxySettingsValid());

        controller.setProxyUser(QStringLiteral("alice"));
        QVERIFY(!controller.proxySettingsValid());

        controller.setProxyPassword(QStringLiteral("secret"));
        QVERIFY(controller.proxySettingsValid());

        controller.setManualProxyType(1);
        QCOMPARE(controller.manualProxyType(), 1);
        QCOMPARE(controller.proxyMode(), 2);
        QVERIFY(controller.proxySettingsValid());

        QVERIFY(proxySpy.count() >= 6);
    }

    void boundsManualProxyPort()
    {
        AccountWizardController controller;

        controller.setProxyPort(0);
        QCOMPARE(controller.proxyPort(), 1);

        controller.setProxyPort(70000);
        QCOMPARE(controller.proxyPort(), 65535);

        controller.setProxyPort(3128);
        QCOMPARE(controller.proxyPort(), 3128);
    }

    void warnsWhenManualProxyTargetsLocalhost_data()
    {
        QTest::addColumn<QString>("serverUrl");
        QTest::addColumn<bool>("warns");

        QTest::newRow("localhost") << QStringLiteral("http://localhost") << true;
        QTest::newRow("localhost without scheme") << QStringLiteral("localhost") << true;
        QTest::newRow("ipv4 loopback") << QStringLiteral("http://127.0.0.1") << true;
        QTest::newRow("ipv4 loopback subnet") << QStringLiteral("http://127.12.34.56") << true;
        QTest::newRow("ipv6 loopback") << QStringLiteral("http://[::1]") << true;
        QTest::newRow("remote host") << QStringLiteral("https://cloud.example") << false;
    }

    void warnsWhenManualProxyTargetsLocalhost()
    {
        QFETCH(QString, serverUrl);
        QFETCH(bool, warns);

        AccountWizardController controller;

        controller.setServerUrl(serverUrl);
        QVERIFY(!controller.showProxyLocalhostWarning());

        controller.setProxyMode(2);
        controller.setProxyHost(QStringLiteral("proxy.example"));

        QCOMPARE(controller.showProxyLocalhostWarning(), warns);
    }

    void systemProxyAndNoProxyDoNotRequireManualSettings()
    {
        AccountWizardController controller;

        controller.setProxyMode(2);
        QVERIFY(!controller.proxySettingsValid());

        controller.setProxyMode(1);
        QCOMPARE(controller.proxyMode(), 1);
        QVERIFY(controller.proxySettingsValid());
        QVERIFY(!controller.showProxyLocalhostWarning());

        controller.setProxyMode(0);
        QCOMPARE(controller.proxyMode(), 0);
        QVERIFY(controller.proxySettingsValid());
    }

    void manualProxyTypeDoesNotSelectManualProxy()
    {
        AccountWizardController controller;

        controller.setProxyMode(0);
        controller.setManualProxyType(0);
        QCOMPARE(controller.proxyMode(), 0);
        QVERIFY(controller.proxySettingsValid());

        controller.setProxyMode(1);
        controller.setManualProxyType(1);
        QCOMPARE(controller.proxyMode(), 1);
        QVERIFY(controller.proxySettingsValid());
    }

    void advancedSafeguardsEmitChanges()
    {
        AccountWizardController controller;
        QSignalSpy askLargeSpy(&controller, &AccountWizardController::askBeforeLargeFoldersChanged);
        QSignalSpy thresholdSpy(&controller, &AccountWizardController::largeFolderThresholdMbChanged);
        QSignalSpy externalSpy(&controller, &AccountWizardController::askBeforeExternalStorageChanged);

        controller.setAskBeforeLargeFolders(!controller.askBeforeLargeFolders());
        controller.setLargeFolderThresholdMb(controller.largeFolderThresholdMb() + 1);
        controller.setAskBeforeExternalStorage(!controller.askBeforeExternalStorage());

        QCOMPARE(askLargeSpy.count(), 1);
        QCOMPARE(thresholdSpy.count(), 1);
        QCOMPARE(externalSpy.count(), 1);
    }

    void exposesProxySettingsAvailability()
    {
        AccountWizardController controller;

        QCOMPARE(controller.proxySettingsAvailable(), !Theme::instance()->doNotUseProxy());
    }

    void preservesForcedOverrideServerChoices()
    {
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto restoreTheme = qScopeGuard([theme, previousOverrideServerUrl, previousForceOverrideServerUrl] {
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
        });

        theme->setOverrideServerUrl(QStringLiteral(
            "["
            R"({"name": "Primary", "url": "https://primary.example"},)"
            R"({"name": "Secondary", "url": "https://secondary.example"})"
            "]"));
        theme->setForceOverrideServerUrl(true);

        AccountWizardController controller;

        QCOMPARE(controller.overrideServerNames(), QStringList({QStringLiteral("Primary"), QStringLiteral("Secondary")}));
        QVERIFY(controller.overrideServerSelectionRequired());
        QCOMPARE(controller.overrideServerIndex(), 0);
        QCOMPARE(controller.serverUrl(), QStringLiteral("https://primary.example"));
        QVERIFY(!controller.serverUrlEditable());

        controller.setOverrideServerIndex(1);

        QCOMPARE(controller.overrideServerIndex(), 1);
        QCOMPARE(controller.serverUrl(), QStringLiteral("https://secondary.example"));
    }

    void startsLoginFlowAutomaticallyForPersistedOverrideServerUrl()
    {
        ConfigFile cfg;
        const auto previousConfiguredOverrideServerUrl = cfg.overrideServerUrl();
        auto theme = Theme::instance();
        const auto previousOverrideServerUrl = theme->overrideServerUrl();
        const auto previousForceOverrideServerUrl = theme->forceOverrideServerUrl();
        const auto previousStartLoginFlowAutomatically = theme->startLoginFlowAutomatically();
        const auto restoreConfiguration = qScopeGuard([
            &cfg,
            theme,
            previousConfiguredOverrideServerUrl,
            previousOverrideServerUrl,
            previousForceOverrideServerUrl,
            previousStartLoginFlowAutomatically
        ] {
            cfg.setOverrideServerUrl(previousConfiguredOverrideServerUrl);
            theme->setOverrideServerUrl(previousOverrideServerUrl);
            theme->setForceOverrideServerUrl(previousForceOverrideServerUrl);
            theme->setStartLoginFlowAutomatically(previousStartLoginFlowAutomatically);
        });

        cfg.setOverrideServerUrl(QStringLiteral("https://cloud.example"));
        theme->setOverrideServerUrl({});
        theme->setForceOverrideServerUrl(false);
        theme->setStartLoginFlowAutomatically(false);

        AccountWizardController controller;

        QCOMPARE(controller.serverUrl(), QStringLiteral("https://cloud.example"));
        QVERIFY(!controller.serverUrlEditable());
        QVERIFY(controller.startLoginFlowAutomatically());
    }
};

QTEST_MAIN(TestAccountWizardController)
#include "testaccountwizardcontroller.moc"
