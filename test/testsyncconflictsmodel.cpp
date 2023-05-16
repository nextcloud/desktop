/*
 * Copyright (C) by Claudio Cambra <claudio.cambra@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
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

private slots:
    void initTestCase()
    {
    }

    void testSettingConflicts()
    {
        auto dir = QTemporaryDir {};
        ConfigFile::setConfDir(dir.path()); // we don't want to pollute the user's config file

        FolderMan fm;

        auto account = Account::create();
        auto url = QUrl{"http://example.de"};
        auto cred = new HttpCredentialsTest("testuser", "secret");
        account->setCredentials(cred);
        account->setUrl(url);
        url.setUserName(cred->user());

        auto newAccountState{AccountStatePtr{ new AccountState{account}}};
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
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ExistingSize)), QString{"6 bytes"});
        QCOMPARE(model.data(model.index(0), static_cast<int>(SyncConflictsModel::SyncConflictRoles::ConflictSize)), QString{"5 bytes"});
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
