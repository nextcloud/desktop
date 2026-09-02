/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include "QApplication"
#include "QDialog"
#include "QLabel"
#include "QTemporaryDir"
#include "account.h"
#include "foldermantestutils.h"
#include "logger.h"
#include "testhelper.h"

#include "accountsettings.h"
#include "ignorelisteditor.h"

using namespace OCC;

class TestAccountSettings : public QObject
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

    void test_globalIgnoreList_hasScopeDescription()
    {
        IgnoreListEditor editor;

        const auto descriptionLabel = editor.findChild<QLabel *>(QStringLiteral("ignorePatternsDescription"));

        QVERIFY(descriptionLabel);
        QCOMPARE(descriptionLabel->text(), QStringLiteral("Global exclusion list for all accounts and synchronized folders"));
    }

    void test_specificIgnoreList_hasScopeDescriptionAndRequestsGlobalEditor()
    {
        auto account = Account::create();
        auto accountState = new FakeAccountState(account);
        AccountSettings settings(accountState);

        QTemporaryDir temporaryFolder(QDir(QDir::tempPath()).filePath(QStringLiteral("Folder & Name-XXXXXX")));
        QVERIFY(temporaryFolder.isValid());

        const auto invoked = QMetaObject::invokeMethod(&settings, "openIgnoredFilesDialog", Qt::DirectConnection, Q_ARG(QString, temporaryFolder.path()));
        QVERIFY(invoked);

        QLabel *descriptionLabel = nullptr;

        for (auto *widget : QApplication::topLevelWidgets()) {
            descriptionLabel = widget->findChild<QLabel *>(QStringLiteral("specificIgnoreListDescription"));

            if (descriptionLabel) {
                break;
            }
        }

        QVERIFY(descriptionLabel);
        QVERIFY(descriptionLabel->text().contains(QFileInfo(temporaryFolder.path()).fileName().toHtmlEscaped()));
        QVERIFY(descriptionLabel->text().contains(QStringLiteral("<a href=\"global-ignore-list\">here</a>")));

        QCOMPARE(descriptionLabel->textFormat(), Qt::RichText);
        QVERIFY(descriptionLabel->wordWrap());
        QVERIFY(!descriptionLabel->openExternalLinks());

        QSignalSpy requestSpy(&settings, &AccountSettings::openGlobalIgnoreListEditorRequested);

        QVERIFY(QMetaObject::invokeMethod(descriptionLabel, "linkActivated", Q_ARG(QString, QStringLiteral("unknown-link"))));
        QCOMPARE(requestSpy.count(), 0);

        QVERIFY(QMetaObject::invokeMethod(descriptionLabel, "linkActivated", Q_ARG(QString, QStringLiteral("global-ignore-list"))));
        QCOMPARE(requestSpy.count(), 1);

        delete descriptionLabel->window();
    }
};

QTEST_MAIN(TestAccountSettings)
#include "testaccountsettings.moc"
