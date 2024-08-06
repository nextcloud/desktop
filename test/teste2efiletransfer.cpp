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

#include <QObject>
#include <QTest>
#include <QSignalSpy>

#include "gui/accountstate.h"
#include "gui/folderman.h"
#include "common/utility.h"
#include "logger.h"

#include "endtoendtestutils.h"

#include <QStandardPaths>

class E2eFileTransferTest : public QObject
{
    Q_OBJECT

public:
    E2eFileTransferTest() = default;

private:

private slots:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);

        QStandardPaths::setTestModeEnabled(true);

        qRegisterMetaType<OCC::SyncResult>("OCC::SyncResult");
    }

    void testSyncFolder()
    {
        {
            EndToEndTestHelper _helper;
            OCC::Folder *_testFolder = nullptr;

            QSignalSpy accountReady(&_helper, &EndToEndTestHelper::accountReady);
            _helper.startAccountConfig();
            QVERIFY(accountReady.wait(3000));

            const auto accountState = _helper.accountState();
            QSignalSpy accountConnected(accountState.data(), &OCC::AccountState::isConnectedChanged);
            QVERIFY(accountConnected.wait(30000));

            _testFolder = _helper.configureSyncFolder();
            QVERIFY(_testFolder);

            // Try the down-sync first
            QSignalSpy folderSyncFinished(_testFolder, &OCC::Folder::syncFinished);
            OCC::FolderMan::instance()->forceSyncForFolder(_testFolder);
            QVERIFY(folderSyncFinished.wait(3000));

            const auto testFolderPath = _testFolder->path();
            const QString expectedFilePath(testFolderPath + QStringLiteral("welcome.txt"));
            const QFile expectedFile(expectedFilePath);
            qDebug() << "Checking if expected file exists at:" << expectedFilePath;
            QVERIFY(expectedFile.exists());

            // Now write a file to test the upload
            const auto fileName = QStringLiteral("test_file.txt");
            const QString localFilePath(_testFolder->path() + fileName);
            QVERIFY(OCC::Utility::writeRandomFile(localFilePath));

            OCC::FolderMan::instance()->forceSyncForFolder(_testFolder);
            QVERIFY(folderSyncFinished.wait(3000));
            qDebug() << "First folder sync complete";

            const auto waitForServerToProcessTime = QTime::currentTime().addSecs(3);
            while (QTime::currentTime() < waitForServerToProcessTime) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            }

            // Do a propfind to check for this file
            const QString remoteFilePath(_testFolder->remotePathTrailingSlash() + fileName);
            auto checkFileExistsJob = new OCC::PropfindJob(_helper.account(), remoteFilePath, this);
            QSignalSpy result(checkFileExistsJob, &OCC::PropfindJob::result);

            checkFileExistsJob->setProperties(QList<QByteArray>() << "getlastmodified");
            checkFileExistsJob->start();
            QVERIFY(result.wait(10000));

            // Now try to delete the file and check change is reflected
            QFile createdFile(localFilePath);
            QVERIFY(createdFile.exists());
            createdFile.remove();

            OCC::FolderMan::instance()->forceSyncForFolder(_testFolder);
            QVERIFY(folderSyncFinished.wait(3000));

            while (QTime::currentTime() < waitForServerToProcessTime) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
            }

            auto checkFileDeletedJob = new OCC::PropfindJob(_helper.account(), remoteFilePath, this);
            QSignalSpy error(checkFileDeletedJob, &OCC::PropfindJob::finishedWithError);

            checkFileDeletedJob->setProperties(QList<QByteArray>() << "getlastmodified");
            checkFileDeletedJob->start();

            QVERIFY(error.wait(10000));
        }

        QTest::qWait(10000);
    }
};

QTEST_MAIN(E2eFileTransferTest)
#include "teste2efiletransfer.moc"
