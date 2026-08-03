/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>

#include <QMenu>
#include <QTemporaryDir>

#include "account.h"
#include "accountmanager.h"
#include "configfile.h"
#include "folder.h"
#include "folderman.h"
#include "systray.h"

#include "foldermantestutils.h"
#include "testhelper.h"

using namespace OCC;

class TestSystraySyncControl : public QObject
{
    Q_OBJECT

    QTemporaryDir _configDir;
    QTemporaryDir _firstFolderDir;
    QTemporaryDir _secondFolderDir;
    FolderManTestHelper _folderManHelper;
    AccountState *_firstAccountState = nullptr;
    AccountState *_secondAccountState = nullptr;
    Folder *_firstFolder = nullptr;
    Folder *_secondFolder = nullptr;

    static AccountState *addTestAccount(const QString &url, const QString &user)
    {
        auto account = Account::create();
        account->setUrl(QUrl(url));
        account->setDavUser(user);
        account->setCredentials(new HttpCredentialsTest(user, QStringLiteral("secret")));
        return AccountManager::instance()->addAccount(account);
    }

#ifndef Q_OS_MACOS
    static QAction *syncControlAction(QMenu &menu, Systray *systray)
    {
        setupQtTrayContextMenu(&menu, systray);
        return menu.findChild<QAction *>(QStringLiteral("traySyncControlAction"));
    }
#endif

private slots:
    void initTestCase()
    {
        QVERIFY(_configDir.isValid());
        QVERIFY(_firstFolderDir.isValid());
        QVERIFY(_secondFolderDir.isValid());

        QStandardPaths::setTestModeEnabled(true);
        ConfigFile::setConfDir(_configDir.path());

        _firstAccountState = addTestAccount(QStringLiteral("https://one.example.com"), QStringLiteral("alice"));
        QVERIFY(_firstAccountState);

        Systray::instance()->create();
    }

    void cleanupTestCase()
    {
        const auto folderMan = FolderMan::instance();
        if (_firstFolder) {
            folderMan->removeFolder(_firstFolder);
        }
        if (_secondFolder) {
            folderMan->removeFolder(_secondFolder);
        }
        _firstFolder = nullptr;
        _secondFolder = nullptr;

        if (_firstAccountState) {
            AccountManager::instance()->removeAccountState(_firstAccountState);
        }
        if (_secondAccountState) {
            AccountManager::instance()->removeAccountState(_secondAccountState);
        }
        _firstAccountState = nullptr;
        _secondAccountState = nullptr;
    }

    void globalActionIsHiddenWithoutClassicFoldersAndTogglesAllFolders()
    {
        const auto systray = Systray::instance();

        // An account without classic folders is also the state used by a File Provider-only client.
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);
        systray->toggleSyncPaused();
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Unavailable);
#ifndef Q_OS_MACOS
        auto unavailableMenu = QMenu{};
        QVERIFY(!syncControlAction(unavailableMenu, systray));
#endif

        _firstFolder = FolderMan::instance()->addFolder(_firstAccountState, folderDefinition(_firstFolderDir.path()));
        QVERIFY(_firstFolder);

        _secondAccountState = addTestAccount(QStringLiteral("https://two.example.com"), QStringLiteral("bob"));
        QVERIFY(_secondAccountState);
        _secondFolder = FolderMan::instance()->addFolder(_secondAccountState, folderDefinition(_secondFolderDir.path()));
        QVERIFY(_secondFolder);

        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);

#ifndef Q_OS_MACOS
        auto pauseMenu = QMenu{};
        const auto pauseAction = syncControlAction(pauseMenu, systray);
        QVERIFY(pauseAction);
        QCOMPARE(pauseAction->text(), Systray::tr("Pause sync for all"));
        pauseAction->trigger();
#else
        systray->toggleSyncPaused();
#endif

        QVERIFY(_firstFolder->syncPaused());
        QVERIFY(_secondFolder->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Resume);

#ifndef Q_OS_MACOS
        auto resumeMenu = QMenu{};
        const auto resumeAction = syncControlAction(resumeMenu, systray);
        QVERIFY(resumeAction);
        QCOMPARE(resumeAction->text(), Systray::tr("Resume sync for all"));
        resumeAction->trigger();
#else
        systray->toggleSyncPaused();
#endif

        QVERIFY(!_firstFolder->syncPaused());
        QVERIFY(!_secondFolder->syncPaused());
        QVERIFY(systray->syncControlState() == Systray::SyncControlState::Pause);
    }
};

QTEST_MAIN(TestSystraySyncControl)
#include "testsystraysynccontrol.moc"
