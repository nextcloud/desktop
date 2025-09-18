/*
 * SPDX-FileCopyrightText: 2023 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2018 ownCloud, Inc.
 * SPDX-License-Identifier: CC0-1.0
 * 
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include <syncengine.h>

using namespace OCC;


class TestSelectiveSync : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        Logger::instance()->setLogFlush(true);
        Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);
    }

    void testSelectiveSyncBigFolders()
    {
        FakeFolder fakeFolder { FileInfo::A12_B12_C12_S12() };
        SyncOptions options;
        options._newBigFolderSizeLimit = 20000; // 20 K
        fakeFolder.syncEngine().setSyncOptions(options);

        QStringList sizeRequests;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation, const QNetworkRequest &req, QIODevice *device)
                                         -> QNetworkReply * {
            // Record what path we are querying for the size
            if (req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == "PROPFIND") {
                if (device->readAll().contains("<size "))
                    sizeRequests << req.url().path();
            }
            return nullptr;
        });

        QSignalSpy newBigFolder(&fakeFolder.syncEngine(), &SyncEngine::newBigFolder);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());

        fakeFolder.remoteModifier().createDir("A/newBigDir");
        fakeFolder.remoteModifier().createDir("A/newBigDir/subDir");
        fakeFolder.remoteModifier().insert("A/newBigDir/subDir/bigFile", options._newBigFolderSizeLimit + 10);
        fakeFolder.remoteModifier().insert("A/newBigDir/subDir/smallFile", 10);

        fakeFolder.remoteModifier().createDir("B/newSmallDir");
        fakeFolder.remoteModifier().createDir("B/newSmallDir/subDir");
        fakeFolder.remoteModifier().insert("B/newSmallDir/subDir/smallFile", 10);

        // Because the test system don't do that automatically
        fakeFolder.remoteModifier().find("A/newBigDir")->extraDavProperties = "<oc:size>20020</oc:size>";
        fakeFolder.remoteModifier().find("A/newBigDir/subDir")->extraDavProperties = "<oc:size>20020</oc:size>";
        fakeFolder.remoteModifier().find("B/newSmallDir")->extraDavProperties = "<oc:size>10</oc:size>";
        fakeFolder.remoteModifier().find("B/newSmallDir/subDir")->extraDavProperties = "<oc:size>10</oc:size>";

        QVERIFY(fakeFolder.syncOnce());

        QCOMPARE(newBigFolder.count(), 1);
        QCOMPARE(newBigFolder.first()[0].toString(), QStringLiteral("A/newBigDir"));
        QCOMPARE(newBigFolder.first()[1].toBool(), false);
        newBigFolder.clear();

        QCOMPARE(sizeRequests.count(), 2); // "A/newBigDir" and "B/newSmallDir";
        QCOMPARE(sizeRequests.filter("/subDir").count(), 0); // at no point we should request the size of the subdirs
        sizeRequests.clear();

        auto oldSync = fakeFolder.currentLocalState();
        // syncing again should do the same
        fakeFolder.syncEngine().journal()->schedulePathForRemoteDiscovery(QStringLiteral("A/newBigDir"));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(fakeFolder.currentLocalState(), oldSync);
        QCOMPARE(newBigFolder.count(), 1); // (since we don't have a real Folder, the files were not added to any list)
        newBigFolder.clear();
        QCOMPARE(sizeRequests.count(), 1); // "A/newBigDir";
        sizeRequests.clear();

        // Simulate that we accept all files by setting a wildcard white list
        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncWhiteList,
            QStringList() << QLatin1String("/"));
        fakeFolder.syncEngine().journal()->schedulePathForRemoteDiscovery(QStringLiteral("A/newBigDir"));
        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(newBigFolder.count(), 0);
        QCOMPARE(sizeRequests.count(), 0);
        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void testRestoreSubFolderForDataFingerPrint()
    {
        const auto mkcolVerb = QByteArray{"MKCOL"};
        FakeFolder fakeFolder{{}};
        fakeFolder.localModifier().mkdir("topFolder");
        fakeFolder.localModifier().mkdir("topFolder/subFolder");
        fakeFolder.localModifier().insert("topFolder/subFolder/a");
        fakeFolder.remoteModifier().extraDavProperties = "<oc:data-fingerprint>initial_finger_print</oc:data-fingerprint>";

        QVERIFY(fakeFolder.syncOnce());

        auto mkdirRequestsCounter = 0;
        fakeFolder.setServerOverride([&mkdirRequestsCounter, mkcolVerb](QNetworkAccessManager::Operation, const QNetworkRequest &req, QIODevice *device)
                                         -> QNetworkReply * {
            Q_UNUSED(device)

            if (req.attribute(QNetworkRequest::CustomVerbAttribute) == mkcolVerb) {
                ++mkdirRequestsCounter;
            }
            qDebug() << req.attribute(QNetworkRequest::CustomVerbAttribute);

            return nullptr;
        });

        fakeFolder.syncEngine().journal()->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList,
                                                                QStringList() << QLatin1String("topFolder"));
        fakeFolder.remoteModifier().extraDavProperties = "<oc:data-fingerprint>changed_finger_print</oc:data-fingerprint>";

        QVERIFY(fakeFolder.syncOnce());
        QCOMPARE(mkdirRequestsCounter, 0);
    }
};

QTEST_GUILESS_MAIN(TestSelectiveSync)
#include "testselectivesync.moc"
