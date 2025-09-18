/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gui/syncconflictsmodel.h"
#include "folderman.h"
#include "accountstate.h"
#include "configfile.h"
#include "syncfileitem.h"

#include "syncenginetestutils.h"
#include "testhelper.h"

#include <QTest>
#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QLocale>

namespace {

QStringList findConflicts(const FileInfo &dir)
{
    QStringList conflicts;
    for (const auto &item : dir.children) {
        if (item.name.contains("(conflicted copy")) {
            conflicts.append(item.path());
        }
    }
    return conflicts;
}

}

using namespace OCC;

class TestSyncConflictsModel : public QObject
{
    Q_OBJECT

private:
    QLocale _locale;

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testSettingConflicts()
    {
        auto dir = QTemporaryDir {};
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file

        FolderMan fm;

        auto account = Account::create();
        auto url = QUrl{"http://example.com"};
        auto cred = new HttpCredentialsTest("testuser", "secret");
        account->setCredentials(cred);
        account->setUrl(url);
        url.setUserName(cred->user());

        auto newAccountState{AccountStatePtr{ new FakeAccountState{account}}};
        auto folderman = FolderMan::instance();
        QCOMPARE(folderman, &fm);

        auto fakeFolder = FakeFolder{FileInfo::A12_B12_C12_S12()};

        QVERIFY(folderman->addFolder(newAccountState.data(), folderDefinition(fakeFolder.localPath())));

        QVERIFY(fakeFolder.syncOnce());

        fakeFolder.localModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");
        fakeFolder.remoteModifier().appendByte("A/a2");

        QVERIFY(fakeFolder.syncOnce());

        OCC::ActivityList allConflicts;

        const auto conflicts = findConflicts(fakeFolder.currentLocalState().children["A"]);
        for (const auto &conflict : conflicts) {
            auto conflictActivity = OCC::Activity{};
            conflictActivity._file = fakeFolder.localPath() + conflict;
            conflictActivity._folder = fakeFolder.localPath();
            allConflicts.push_back(std::move(conflictActivity));
        }

        SyncConflictsModel model;
        QAbstractItemModelTester modelTester(&model);

        model.setConflictActivities(allConflicts);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ExistingFileName)), QString{"a2"});
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ExistingSize)), _locale.formattedDataSize(6));
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ConflictSize)), _locale.formattedDataSize(5));
        QVERIFY(!model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ExistingDate)).toString().isEmpty());
        QVERIFY(!model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ConflictDate)).toString().isEmpty());
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ExistingPreviewUrl)), QVariant::fromValue(QUrl{QStringLiteral("image://tray-image-provider/:/fileicon%1A/a2").arg(fakeFolder.localPath())}));
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ConflictPreviewUrl)), QVariant::fromValue(QUrl{QStringLiteral("image://tray-image-provider/:/fileicon%1%2").arg(fakeFolder.localPath(), conflicts.constFirst())}));
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ExistingSelected)), false);
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ConflictSelected)), false);
    }
};

QTEST_GUILESS_MAIN(TestSyncConflictsModel)
#include "testsyncconflictsmodel.moc"
