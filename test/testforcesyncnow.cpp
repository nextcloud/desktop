/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtTest>
#include <QTemporaryDir>

#include "account.h"
#include "accountstate.h"
#include "configfile.h"
#include "folder.h"
#include "folderman.h"
#include "logger.h"
#include "tray/usermodel.h"

#include "foldermantestutils.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace OCC;

/// Coverage for `User::forceSyncNow()`. Two cases:
///
///  - Classic-sync branch: a `Folder` exists for the account, so `forceSyncNow()` must route
///    through `FolderMan::forceSyncForFolder()`, which unpauses the folder. We observe the
///    state flip.
///  - Folderless branch: no `Folder` and no File Provider domain. The pre-fix implementation
///    crashed at `folderman.cpp:792` (dereferencing a null `Folder*`). The fix added an early
///    return; we lock that in by verifying no crash.
///
/// The FPE branch (`hasFileProvider() && Mac::FileProvider::instance()->domainManager()...`) is
/// only reachable when `BUILD_FILE_PROVIDER_MODULE` is on and an `NSFileProviderDomain` is
/// registered with the OS. That requires macOS plus a configured account, so it is exercised
/// via the manual verification steps in the issue rather than here.
class TestForceSyncNow : public QObject
{
    Q_OBJECT

    FolderManTestHelper _folderManHelper;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);
        QStandardPaths::setTestModeEnabled(true);
    }

    void forceSyncNow_withClassicFolder_unpausesAndSchedules()
    {
        QTemporaryDir confDir;
        ConfigFile::setConfDir(confDir.path()); // don't pollute the user's config

        auto account = Account::create();
        account->setCredentials(new HttpCredentialsTest("testuser", "secret"));
        account->setUrl(QUrl("http://example.com"));

        auto accountState = AccountStatePtr{new FakeAccountState{account}};

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        auto folderman = FolderMan::instance();
        const auto folder = folderman->addFolder(accountState.data(), folderDefinition(fakeFolder.localPath()));
        QVERIFY(folder);

        // Put the folder in a state that `forceSyncForFolder` is supposed to clear.
        folder->setSyncPaused(true);
        QVERIFY(folder->syncPaused());

        User user(accountState);
        QVERIFY(user.hasLocalFolder());

        user.forceSyncNow();

        // `FolderMan::forceSyncForFolder` calls `setSyncPaused(false)` — observe the flip as
        // proof that `User::forceSyncNow()` routed through the classic path.
        QVERIFY(!folder->syncPaused());
    }

    void forceSyncNow_withoutFolderOrFileProvider_doesNotCrash()
    {
        QTemporaryDir confDir;
        ConfigFile::setConfDir(confDir.path());

        auto account = Account::create();
        account->setCredentials(new HttpCredentialsTest("testuser", "secret"));
        account->setUrl(QUrl("http://example.com"));

        auto accountState = AccountStatePtr{new FakeAccountState{account}};

        // No folder added to FolderMan for this account, and no File Provider domain is set
        // (`fileProviderDomainIdentifier()` is empty by default), so `hasFileProvider()` is
        // false. Pre-fix this would call `FolderMan::forceSyncForFolder(nullptr)` and crash.
        User user(accountState);
        QVERIFY(!user.hasLocalFolder());

        user.forceSyncNow(); // must not crash

        QVERIFY(true);
    }
};

QTEST_MAIN(TestForceSyncNow)
#include "testforcesyncnow.moc"
