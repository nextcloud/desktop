/*
 * SPDX-FileCopyrightText: 2025 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

#include "accountstate.h"
#include "common/vfs.h"
#include "csync/csync_exclude.h"
#include "folder.h"

#include "account.h"
#include "accountmanager.h"
#include "logger.h"
#include "syncengine.h"
#include "theme.h"

using namespace Qt::StringLiterals;

using namespace OCC;

namespace
{
class PinStateTrackingVfs : public VfsOff
{
public:
    bool setPinState(const QString &relativePath, PinState state) override
    {
        pinStates.insert(relativePath, state);
        return true;
    }

    Optional<PinState> pinState(const QString &relativePath) override
    {
        const auto found = pinStates.constFind(relativePath);
        if (found == pinStates.cend()) {
            return {};
        }
        return *found;
    }

    QHash<QString, PinState> pinStates;
};
} // namespace

class TestFolder: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

#ifdef Q_OS_MACOS
    void test_reauthorizationPathMatching()
    {
        const auto composedPath = u"/Users/test/sciebo - \u00DCser, Name (user@example.org)@sciebo.example.org/"_s;
        const auto decomposedPath = u"/Users/test/sciebo - U\u0308ser, Name (user@example.org)@sciebo.example.org"_s;

        QVERIFY(FolderDefinition::localPathsMatch(composedPath, composedPath));
        QVERIFY(FolderDefinition::localPathsMatch(composedPath, decomposedPath));
        QVERIFY(FolderDefinition::localPathsMatch(decomposedPath, composedPath));
        QVERIFY(!FolderDefinition::localPathsMatch(composedPath, decomposedPath + u"-other"_s));
        QVERIFY(!FolderDefinition::localPathsMatch(composedPath, decomposedPath.toLower()));
    }
#endif

    void testWatchedIgnoredPathUsesSyncRoot_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("pattern");
        QTest::addColumn<bool>("directory");
        QTest::addColumn<bool>("exists");
        QTest::addColumn<bool>("workingDirectoryDecoy");
        QTest::newRow("desktop.ini") << u"desktop.ini"_s << QString{} << false << true << false;
        QTest::newRow("DS_Store file") << u".DS_Store"_s << u"].DS_Store"_s << false << true << false;
        QTest::newRow("DS_Store directory") << u".DS_Store"_s << u"].DS_Store"_s << true << true << false;
        QTest::newRow("custom ignored directory") << u"local-only"_s << u"local-only/"_s << true << true << false;
        QTest::newRow("removed ignored file") << u"removed.ignored"_s << u"removed.ignored"_s << false << false << false;
        QTest::newRow("decoy in working directory") << u"removed.ignored"_s << u"removed.ignored"_s << false << false << true;
    }

    void testWatchedIgnoredPathUsesSyncRoot()
    {
        QFETCH(QString, name);
        QFETCH(QString, pattern);
        QFETCH(bool, directory);
        QFETCH(bool, exists);
        QFETCH(bool, workingDirectoryDecoy);

        const auto root = QTemporaryDir{};
        QVERIFY(root.isValid());
        const QString relativePath = u"nested/"_s + name;
        const auto absolutePath = root.filePath(relativePath);
        QVERIFY(QDir(root.path()).mkdir(u"nested"_s));
        if (exists && directory) {
            QVERIFY(QDir().mkdir(absolutePath));
        } else if (exists) {
            auto file = QFile(absolutePath);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        auto account = Account::create();
        account->setUrl(QUrl(u"http://localhost"_s));
        const auto accountState = AccountStatePtr(new AccountState(account));
        auto definition = FolderDefinition{};
        definition.localPath = root.path();
        definition.targetPath = u"/"_s;
        auto vfs = std::make_unique<PinStateTrackingVfs>();
        auto *trackingVfs = vfs.get();
        auto folder = Folder(definition, accountState.data(), std::move(vfs));
        if (!pattern.isEmpty()) {
            folder.syncEngine().excludedFiles().addManualExclude(pattern);
        }
        const auto workingDirectory = QTemporaryDir{};
        QVERIFY(workingDirectory.isValid());
        const auto originalWorkingDirectory = QDir::currentPath();
        const auto restoreWorkingDirectory = qScopeGuard([&] {
            QDir::setCurrent(originalWorkingDirectory);
        });
        QVERIFY(QDir::setCurrent(workingDirectory.path()));
        if (workingDirectoryDecoy) {
            QVERIFY(QDir().mkdir(u"nested"_s));
            auto decoyFile = QFile(relativePath);
            QVERIFY(decoyFile.open(QIODevice::WriteOnly));
        }
        QCOMPARE(QFileInfo::exists(relativePath), workingDirectoryDecoy);

        const QString watchedPath = folder.path() + relativePath;
        QCOMPARE(QFileInfo::exists(watchedPath), exists);
        QVERIFY(folder.pathIsIgnored(watchedPath));
        folder.slotWatchedPathChanged(watchedPath, Folder::ChangeReason::Other);

        if (exists) {
            QCOMPARE(trackingVfs->pinStates.size(), 1);
            QCOMPARE(trackingVfs->pinStates.value(relativePath), PinState::Excluded);
        } else {
            QVERIFY(trackingVfs->pinStates.isEmpty());
        }
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
