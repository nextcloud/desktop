/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include <QTemporaryDir>

#include "accountstate.h"
#include "common/vfs.h"
#include "folder.h"

#include "account.h"
#include "accountmanager.h"
#include "logger.h"
#include "theme.h"

using namespace Qt::StringLiterals;

using namespace OCC;

class TestFolder: public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void test_sidebarDisplayName()
    {
        const auto createAccount = [](const QString &url, const QString &displayName) -> AccountState * {
            auto account = Account::create();
            account->setUrl(QUrl(url));
            account->setDavDisplayName(displayName);
            return AccountManager::instance()->addAccount(account);
        };

        const auto createFolder = [](AccountState * const accountState, const QString &targetPath, const QString &alias) -> Folder {
            FolderDefinition definition;
            definition.localPath = QTemporaryDir().path(); // for these tests the local folder path doesn't matter
            definition.targetPath = targetPath; // also known as the remote path
            definition.alias = alias;

            return Folder(definition, accountState, createVfsFromPlugin(Vfs::Off));
        };

        const auto appName = Theme::instance()->appNameGUI();

        // at this stage only one account is created
        const auto account1 = createAccount("http://admin:admin@nextcloud.local"_L1, "Admin"_L1);
        QVERIFY(account1);

        // when syncing the root folder it should return the (branded) app name
        auto folder1 = createFolder(account1, "/"_L1, "1"_L1);
        QCOMPARE(folder1.sidebarDisplayName(), appName);

        // when syncing subfolders it should return the remote folder path without the leading slash
        auto folder2 = createFolder(account1, "/Documents"_L1, "2"_L1);
        QCOMPARE(folder2.sidebarDisplayName(), "Documents"_L1);
        auto folder3 = createFolder(account1, "/Photos/to_sort"_L1, "3"_L1);
        QCOMPARE(folder3.sidebarDisplayName(), "Photos/to_sort"_L1);

        // so far we only had one account, let's add another one!
        const auto account2 = createAccount("http://testuser:testuser@stable31.local"_L1, "Test user"_L1);
        const auto folder4 = createFolder(account2, "/"_L1, "1"_L1);

        const auto expectedSuffixAccount1 = " - nextcloud.local - Admin"_L1;
        const auto expectedSuffixAccount2 = " - stable31.local - Test user"_L1;

        // now the account-specific suffixes should be added
        QCOMPARE(folder1.sidebarDisplayName(), appName + expectedSuffixAccount1);
        QCOMPARE(folder2.sidebarDisplayName(), "Documents"_L1 + expectedSuffixAccount1);
        QCOMPARE(folder3.sidebarDisplayName(), "Photos/to_sort"_L1 + expectedSuffixAccount1);
        QCOMPARE(folder4.sidebarDisplayName(), appName + expectedSuffixAccount2);
    }
};

QTEST_GUILESS_MAIN(TestFolder)
#include "testfolder.moc"
