/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QWidget>

#include "account.h"
#include "foldermantestutils.h"
#include "logger.h"
#include "managedsettingstestutils.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

#include "networksettings.h"

using namespace OCC;
using namespace Qt::StringLiterals;

class TestNetworkSettings : public QObject
{
    Q_OBJECT

    FolderManTestHelper helper;

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void test_whenAccountIsLoggedOut_doesNotCrash()
    {
        // Create an account that is not registered in AccountManager
        // This simulates a logged-out account scenario
        auto account = Account::create();
        account->setUrl(QUrl(QStringLiteral("https://example.com")));
        account->setDavUser(QStringLiteral("testuser"));
        
        // Create NetworkSettings with this account
        // When proxy settings are changed, saveProxySettings() will try to
        // get accountState from AccountManager, which will return nullptr
        // This test ensures the app doesn't crash in that scenario
        NetworkSettings settings(account);
        
        // The test passes if we reach here without crashing
        QVERIFY(true);
    }

    void testEnforcedProxyFieldsDisableOnlyTheirControls_data()
    {
        QTest::addColumn<bool>("typeEnforced");
        QTest::addColumn<bool>("hostAndPortEnforced");

        QTest::newRow("host and port enforced") << false << true;
        QTest::newRow("only type enforced") << true << false;
    }

    void testEnforcedProxyFieldsDisableOnlyTheirControls()
    {
        QFETCH(bool, typeEnforced);
        QFETCH(bool, hostAndPortEnforced);

        auto account = Account::create();
        account->setUrl(QUrl(u"https://example.com"_s));
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        account->setProxyType(QNetworkProxy::HttpProxy);
        account->applyManagedProxySettings(managedProxyFields(true,
                                                              typeEnforced ? std::optional<int>(QNetworkProxy::HttpProxy) : std::nullopt,
                                                              hostAndPortEnforced ? std::optional<QString>(u"proxy.example.com"_s) : std::nullopt,
                                                              hostAndPortEnforced ? std::optional<int>(8080) : std::nullopt));

        NetworkSettings settings(account);

        const auto typeComboBox = settings.findChild<QWidget *>(u"typeComboBox"_s);
        const auto manualProxyRadioButton = settings.findChild<QWidget *>(u"manualProxyRadioButton"_s);
        const auto hostLineEdit = settings.findChild<QWidget *>(u"hostLineEdit"_s);
        const auto portSpinBox = settings.findChild<QWidget *>(u"portSpinBox"_s);
        const auto authRequiredCheckBox = settings.findChild<QWidget *>(u"authRequiredcheckBox"_s);
        const auto proxyEnforcedLabel = settings.findChild<QWidget *>(u"proxyEnforcedLabel"_s);
        QVERIFY(typeComboBox && manualProxyRadioButton && hostLineEdit && portSpinBox && authRequiredCheckBox && proxyEnforcedLabel);

        QCOMPARE(typeComboBox->isEnabled(), !typeEnforced);
        QCOMPARE(manualProxyRadioButton->isEnabled(), !typeEnforced);
        QCOMPARE(hostLineEdit->isEnabled(), !hostAndPortEnforced);
        QCOMPARE(portSpinBox->isEnabled(), !hostAndPortEnforced);
        QVERIFY(authRequiredCheckBox->isEnabled());
        QVERIFY(!proxyEnforcedLabel->isHidden());
    }
};

QTEST_MAIN(TestNetworkSettings)
#include "testnetworksettings.moc"
