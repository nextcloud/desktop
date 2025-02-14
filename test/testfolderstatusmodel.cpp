/*
 * Copyright (C) by Matthieu Gallien <matthieu.gallien@nextcloud.com>
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

#include <QtTest>
#include <QStandardPaths>
#include <QAbstractItemModelTester>
#include <theme.h>

#include "accountstate.h"
#include "folderman.h"
#include "folderstatusmodel.h"
#include "logger.h"
#include "syncenginetestutils.h"
#include "testhelper.h"

using namespace OCC;
using namespace OCC::Utility;

class TestFolderStatusModel : public QObject
{
    Q_OBJECT
    std::unique_ptr<FolderMan> _folderMan;

public:

private Q_SLOTS:
    void initTestCase()
    {
        Logger::instance()->setLogFlush(true);
        Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        _folderMan.reset(new FolderMan{});
    }
    
    void testThatFolderManHasInstance() {
        QVERIFY2(nullptr != FolderMan::instance(), "folder manager must not be a NULL");
    }
    
    void testEmptyModelHasOneColumn()
    {
        const FolderStatusModel test;
        QCOMPARE(test.columnCount(), 1);
    }
    
    void testAccountStateIsConnected()
    {
        const auto fakeFolder = makeFakeFolder();
        const FakeAccountState accountState{fakeFolder.account()};
        QVERIFY2(accountState.isConnected(), "account must be connected");
    }
    
    void testAbilityToAddRemoveAccountFolders()
    {
        executeFolderManTest([](const AccountState *accountState, Folder* folder){
            QVERIFY2(folder, "folder manager was not able to add folder for a given account");
            QCOMPARE(folder->accountState(), accountState);
        });
    }
    
    void testModelConsistencyWithFakeFolder()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel& test) {
            test.setAccountState(accountState);
            QVERIFY2(!test.isDirty(), "model is dirty after set account state");
        });
    }
    
    void testNewModelEmitDirtyChangedSignalAfterSetAccountState()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel& test) {
            const QSignalSpy dirtySignalSpy{&test, &FolderStatusModel::dirtyChanged};
            test.setAccountState(accountState);
            QCOMPARE(dirtySignalSpy.count(), 1);
        });
    }
    
    void testModelRowsCount()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel& test) {
            test.setAccountState(accountState);
            // folders count +1 for the "add folder" button if [singleSyncFolder] is true
            const auto expectedRowsCount = Theme::instance()->singleSyncFolder() ? 1 : 2;
            QCOMPARE(test.rowCount(), expectedRowsCount);
        });
    }
    
    void testRowsChildrenConsistency()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel &test) {
            test.setAccountState(accountState);
            QVERIFY2(test.hasChildren({}), "empty index always has children");
            // we assume that folder may contains of some child elements until not yet fetched
            QCOMPARE(test.hasChildren(test.index(0)), !test._folders.front()._fetched);
        });
    }
    
    void testThatRowClassifyWorksProperly()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel& test) {
            test.setAccountState(accountState);
            QCOMPARE(test.classify({}), FolderStatusModel::ItemType::AddButton);
            QCOMPARE(test.classify(test.index(0)), FolderStatusModel::ItemType::RootFolder);
        });
    }
    
    void testSubFoldersInfoForIndices()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel& test) {
            test.setAccountState(accountState);
            QCOMPARE(test.infoForIndex({}), nullptr);
            QVERIFY2(nullptr != test.infoForIndex(test.index(0)), "NULL info for index 0");
        });
    }
    
    void testThatAppropriateSignalsEmittedIfItemChecked()
    {
        executeConsistencyModelTest([](const AccountState *accountState, FolderStatusModel& test) {
            test.setAccountState(accountState);
            // set checked for our single root folder
            const QSignalSpy dataChangedSignalSpy{&test, &QAbstractItemModel::dataChanged};
            const QSignalSpy dirtySignalSpy{&test, &FolderStatusModel::dirtyChanged};
            QVERIFY(test.setData(test.index(0), Qt::CheckState::Checked, Qt::CheckStateRole));
            QVERIFY2(dataChangedSignalSpy.count() > 0, "make sure the signal [dataChanged] was emitted at least one time");
            QVERIFY2(dirtySignalSpy.count() > 0, "make sure the signal [dirtyChanged] was emitted at least one time");
            QVERIFY2(test.isDirty(), "model is not dirty after set checked state for the folder");
        });
    }

private:
    static FakeFolder makeFakeFolder()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};

        const auto account = fakeFolder.account();
        const auto capabilities = QVariantMap {
                                              {QStringLiteral("end-to-end-encryption"), QVariantMap {
                                                                                            {QStringLiteral("enabled"), true},
                                                                                            {QStringLiteral("api-version"), QString::number(2.0)},
                                                                                            }},
                                              };
        account->setCapabilities(capabilities);
        account->setCredentials(new FakeCredentials{fakeFolder.networkAccessManager()});
        account->setUrl(QUrl(("owncloud://somehost/owncloud")));
        return fakeFolder;
    }
    
    static OCC::FolderDefinition makeFolderDefinition(const QString &path) {
        auto folderDef = folderDefinition(path);
        folderDef.targetPath.clear();
        return folderDef;
    }
    
    static OCC::FolderDefinition makeFolderDefinition(const FakeFolder &folder) {
        return makeFolderDefinition(folder.localPath());
    }
    
    template <typename TestFunctor>
    static void executeFolderManTest(TestFunctor&& functor)
    {
        const auto fakeFolder = makeFakeFolder();
        const AccountStatePtr accountState{new FakeAccountState{fakeFolder.account()}};
        const auto folder = FolderMan::instance()->addFolder(accountState.get(),
                                                             makeFolderDefinition(fakeFolder));
        try {
            functor(accountState.get(), folder);
            // cleanup
            if (folder) {
                FolderMan::instance()->removeFolder(folder);
            }
        }
        catch(...) {
            // cleanup
            if (folder) {
                FolderMan::instance()->removeFolder(folder);
            }
            throw; // rethrow exception from test framework
        }
    }
    
    template <typename TestFunctor>
    static void executeConsistencyModelTest(TestFunctor&& functor)
    {
        executeFolderManTest([functor](const AccountState *accountState, Folder*){
            FolderStatusModel test;
            QAbstractItemModelTester tester{&test, QAbstractItemModelTester::FailureReportingMode::QtTest};
            tester.setUseFetchMore(true);
            functor(accountState, test);
        });
    }
};

QTEST_MAIN(TestFolderStatusModel)
#include "testfolderstatusmodel.moc"
