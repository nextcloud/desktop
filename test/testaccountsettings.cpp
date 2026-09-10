/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>

#include "account.h"
#include "foldermantestutils.h"
#include "logger.h"
#include "syncenginetestutils.h"
#include "testhelper.h"
#include "theme.h"

#include "accountsettings.h"

using namespace OCC;

class TestAccountSettings : public QObject
{
    Q_OBJECT

    FolderManTestHelper helper;

    static QVariantMap
    shortcutCapabilities(const bool userStatusEnabled, const bool assistantEnabled, const QString &assistantVersion = QStringLiteral("1.0.9"))
    {
        return {
            {QStringLiteral("user_status"), QVariantMap{{QStringLiteral("enabled"), userStatusEnabled}}},
            {QStringLiteral("assistant"),
             QVariantMap{
                 {QStringLiteral("enabled"), assistantEnabled},
                 {QStringLiteral("version"), assistantVersion},
             }},
        };
    }

    static QPushButton *shortcutButton(const AccountSettings &settings, const char *objectName)
    {
        return settings.findChild<QPushButton *>(QString::fromLatin1(objectName));
    }

    /** @brief Creates an account backed by the test network access manager. */
    static AccountPtr accountWithFakeNetworkAccessManager()
    {
        auto account = Account::create();
        account->setCredentials(new FakeCredentials{new FakeQNAM({})});
        return account;
    }

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void test_whenAccountStateIsNotConnected_doesNotCrash()
    {
        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        accountState->setStateForTesting(OCC::AccountState::SignedOut);
        QCOMPARE_EQ(accountState->state(), OCC::AccountState::SignedOut);
        AccountSettings a(accountState);
    }

    void test_whenAccountStateIsConnected_doesNotCrash()
    {
        // this occurred because ConnectionValidator used to set the account
        // inside a Account's _e2e member, instead of letting Account itself
        // do that.

        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        QCOMPARE_EQ(accountState->state(), OCC::AccountState::Connected);
        AccountSettings a(accountState);
    }

    void test_accountSectionsFollowExpectedOrder()
    {
        auto account = Account::create();
        auto accountState = FakeAccountState(account);
        AccountSettings settings(&accountState);

        const auto layout = settings.findChild<QVBoxLayout *>(QStringLiteral("verticalLayout_2"));
        const auto shortcutsPanel = settings.findChild<QWidget *>(QStringLiteral("accountShortcutsPanel"));
        const auto syncFoldersPanel = settings.findChild<QWidget *>(QStringLiteral("syncFoldersPanel"));
        const auto fileProviderPanel = settings.findChild<QWidget *>(QStringLiteral("fileProviderMaintenancePanel"));
        const auto encryptionPanel = settings.findChild<QWidget *>(QStringLiteral("encryptionPanel"));
        const auto connectionPanel = settings.findChild<QWidget *>(QStringLiteral("accountStatusPanel"));
        const auto accountActionsPanel = settings.findChild<QWidget *>(QStringLiteral("accountActionsPanel"));
        QVERIFY(layout);
        QVERIFY(shortcutsPanel);
        QVERIFY(syncFoldersPanel);
        QVERIFY(fileProviderPanel);
        QVERIFY(qobject_cast<QFrame *>(fileProviderPanel));
        QVERIFY(encryptionPanel);
        QVERIFY(connectionPanel);
        QVERIFY(accountActionsPanel);

        QVERIFY(layout->indexOf(shortcutsPanel) < layout->indexOf(syncFoldersPanel));
        QVERIFY(layout->indexOf(syncFoldersPanel) < layout->indexOf(fileProviderPanel));
        QVERIFY(layout->indexOf(fileProviderPanel) < layout->indexOf(encryptionPanel));
        QVERIFY(layout->indexOf(encryptionPanel) < layout->indexOf(connectionPanel));
        QVERIFY(layout->indexOf(connectionPanel) < layout->indexOf(accountActionsPanel));
        QCOMPARE(fileProviderPanel->layout()->contentsMargins(), shortcutsPanel->layout()->contentsMargins());
    }

    void test_accountShortcutsFollowConnectionState()
    {
        auto account = accountWithFakeNetworkAccessManager();
        account->setCapabilities(shortcutCapabilities(true, true));
        auto accountState = FakeAccountState(account);
        AccountSettings settings(&accountState);

        const auto activitiesButton = shortcutButton(settings, "activitiesShortcutButton");
        const auto userStatusButton = shortcutButton(settings, "userStatusShortcutButton");
        const auto assistantButton = shortcutButton(settings, "assistantShortcutButton");
        const auto searchButton = shortcutButton(settings, "searchShortcutButton");
        QVERIFY(activitiesButton);
        QVERIFY(userStatusButton);
        QVERIFY(assistantButton);
        QVERIFY(searchButton);
        QVERIFY(!activitiesButton->isHidden());
        QVERIFY(!userStatusButton->isHidden());
        QVERIFY(!assistantButton->isHidden());
        QVERIFY(!searchButton->isHidden());

        accountState.setStateForTesting(AccountState::SignedOut);
        QVERIFY(activitiesButton->isHidden());
        QVERIFY(userStatusButton->isHidden());
        QVERIFY(assistantButton->isHidden());
        QVERIFY(searchButton->isHidden());

        accountState.setStateForTesting(AccountState::Connected);
        QVERIFY(!activitiesButton->isHidden());
        QVERIFY(!userStatusButton->isHidden());
        QVERIFY(!assistantButton->isHidden());
        QVERIFY(!searchButton->isHidden());
    }

    void test_accountShortcutLabelsMatchDedicatedWindows()
    {
        auto account = Account::create();
        auto accountState = FakeAccountState(account);
        AccountSettings settings(&accountState);

        QCOMPARE(shortcutButton(settings, "activitiesShortcutButton")->text(), QCoreApplication::translate("ActivitiesWindow", "Activities"));
        QCOMPARE(shortcutButton(settings, "userStatusShortcutButton")->text(), QCoreApplication::translate("UserStatusWindow", "Online status"));
        QCOMPARE(shortcutButton(settings, "assistantShortcutButton")->text(), QCoreApplication::translate("AssistantWindow", "Assistant"));
        QCOMPARE(shortcutButton(settings, "searchShortcutButton")->text(), QCoreApplication::translate("SearchWindow", "Search"));
    }

    void test_assistantShortcutIconUsesLightPaletteForeground()
    {
        auto account = accountWithFakeNetworkAccessManager();
        account->setCapabilities(shortcutCapabilities(true, true));
        auto accountState = FakeAccountState(account);
        auto parent = QWidget{};
        auto lightPalette = parent.palette();
        lightPalette.setColor(QPalette::Base, Qt::white);
        parent.setPalette(lightPalette);
        AccountSettings settings(&accountState, &parent);

        const auto assistantButton = shortcutButton(settings, "assistantShortcutButton");
        QVERIFY(assistantButton);

        const auto iconImage = assistantButton->icon().pixmap(QSize(24, 24)).toImage();
        QVERIFY(!iconImage.isNull());
        const auto iconCenter = iconImage.pixelColor(iconImage.width() * 3 / 8, iconImage.height() / 2);
        QVERIFY(iconCenter.alpha() > 0);
        QVERIFY(Theme::isDarkColor(iconCenter));
    }

    void test_accountShortcutsFollowLiveCapabilities()
    {
        auto account = accountWithFakeNetworkAccessManager();
        auto accountState = FakeAccountState(account);
        AccountSettings settings(&accountState);

        const auto activitiesButton = shortcutButton(settings, "activitiesShortcutButton");
        const auto userStatusButton = shortcutButton(settings, "userStatusShortcutButton");
        const auto assistantButton = shortcutButton(settings, "assistantShortcutButton");
        const auto searchButton = shortcutButton(settings, "searchShortcutButton");
        QVERIFY(activitiesButton);
        QVERIFY(userStatusButton);
        QVERIFY(assistantButton);
        QVERIFY(searchButton);
        QVERIFY(!activitiesButton->isHidden());
        QVERIFY(userStatusButton->isHidden());
        QVERIFY(assistantButton->isHidden());
        QVERIFY(!searchButton->isHidden());

        account->setCapabilities(shortcutCapabilities(true, true, QStringLiteral("1.0.8")));
        QVERIFY(!userStatusButton->isHidden());
        QVERIFY(assistantButton->isHidden());

        account->setCapabilities(shortcutCapabilities(true, true));
        QVERIFY(!userStatusButton->isHidden());
        QVERIFY(!assistantButton->isHidden());

        account->setCapabilities(shortcutCapabilities(false, false));
        QVERIFY(userStatusButton->isHidden());
        QVERIFY(assistantButton->isHidden());
    }

    void test_accountShortcutButtonsEmitAccountSpecificRequests()
    {
        auto account = accountWithFakeNetworkAccessManager();
        account->setCapabilities(shortcutCapabilities(true, true));
        auto accountState = FakeAccountState(account);
        AccountSettings settings(&accountState);

        AccountState *activitiesAccount = nullptr;
        AccountState *userStatusAccount = nullptr;
        AccountState *assistantAccount = nullptr;
        AccountState *searchAccount = nullptr;
        connect(&settings, &AccountSettings::showIssuesList, this, [&activitiesAccount](AccountState *requestedAccount) {
            activitiesAccount = requestedAccount;
        });
        connect(&settings, &AccountSettings::showUserStatus, this, [&userStatusAccount](AccountState *requestedAccount) {
            userStatusAccount = requestedAccount;
        });
        connect(&settings, &AccountSettings::showAssistant, this, [&assistantAccount](AccountState *requestedAccount) {
            assistantAccount = requestedAccount;
        });
        connect(&settings, &AccountSettings::showSearch, this, [&searchAccount](AccountState *requestedAccount) {
            searchAccount = requestedAccount;
        });

        shortcutButton(settings, "activitiesShortcutButton")->click();
        shortcutButton(settings, "userStatusShortcutButton")->click();
        shortcutButton(settings, "assistantShortcutButton")->click();
        shortcutButton(settings, "searchShortcutButton")->click();

        QCOMPARE(activitiesAccount, &accountState);
        QCOMPARE(userStatusAccount, &accountState);
        QCOMPARE(assistantAccount, &accountState);
        QCOMPARE(searchAccount, &accountState);
    }
};

QTEST_MAIN(TestAccountSettings)
#include "testaccountsettings.moc"
