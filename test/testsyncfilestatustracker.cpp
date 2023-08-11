/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "testutils/syncenginetestutils.h"
#include "csync_exclude.h"

using namespace OCC;

class StatusPushSpy : public QSignalSpy
{
    SyncEngine &_syncEngine;
public:
    StatusPushSpy(SyncEngine &syncEngine)
        : QSignalSpy(&syncEngine.syncFileStatusTracker(), &SyncFileStatusTracker::fileStatusChanged)
        , _syncEngine(syncEngine)
    {
    }

    SyncFileStatus statusOf(const QString &relativePath) const
    {
        const QFileInfo file(_syncEngine.localPath(), relativePath);
        // Start from the end to get the latest status
        for (auto it = crbegin(); it != crend(); ++it) {
            const auto info = QFileInfo(it->at(0).toString());
            if (info == file) {
                return it->at(1).value<SyncFileStatus>();
            }
        }
        return SyncFileStatus();
    }

    bool statusEmittedBefore(const QString &firstPath, const QString &secondPath) const {
        QFileInfo firstFile(_syncEngine.localPath(), firstPath);
        QFileInfo secondFile(_syncEngine.localPath(), secondPath);
        // Start from the end to get the latest status
        int i = size() - 1;
        for (; i >= 0; --i) {
            if (QFileInfo(at(i)[0].toString()) == secondFile)
                break;
            else if (QFileInfo(at(i)[0].toString()) == firstFile)
                return false;
        }
        for (; i >= 0; --i) {
            if (QFileInfo(at(i)[0].toString()) == firstFile)
                return true;
        }
        return false;
    }
};

//
// IMPORTANT: these tests are only run with VFS off, because the tests assume a very controlled order of status changes.
// When using some form of VFS, the event loop has to run in order to fulfill requests from the OS, and therefore things
// like stopping execution in `execUntilBeforePropagation()` are very hard to do.
//
class TestSyncFileStatusTracker : public QObject
{
    Q_OBJECT

    void verifyThatPushMatchesPull(const FakeFolder &fakeFolder, const StatusPushSpy &statusSpy) {
        QString root = fakeFolder.localPath();
        QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next().mid(root.size());
            auto pushedStatus = statusSpy.statusOf(filePath);
            if (pushedStatus != SyncFileStatus()) {
                QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(filePath), pushedStatus);
            }
        }
    }

private slots:
    void parentsGetSyncStatusUploadDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().appendByte(QStringLiteral("B/b1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        fakeFolder.remoteModifier().appendByte(QStringLiteral("C/c1"));
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("B/b2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("C/c2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusNewFileUploadDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().insert(QStringLiteral("B/b0"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        fakeFolder.remoteModifier().insert(QStringLiteral("C/c0"));
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c0")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("C/c1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c0")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusNewDirDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().mkdir(QStringLiteral("D"));
        fakeFolder.remoteModifier().insert(QStringLiteral("D/d0"));
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D/d0")), SyncFileStatus(SyncFileStatus::StatusSync));

        statusSpy.clear();
        fakeFolder.execUntilItemCompleted(QStringLiteral("D"));
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D/d0")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D")), SyncFileStatus(SyncFileStatus::StatusNone));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D/d0")), SyncFileStatus(SyncFileStatus::StatusNone));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusNewDirUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().mkdir(QStringLiteral("D"));
        fakeFolder.localModifier().insert(QStringLiteral("D/d0"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D/d0")), SyncFileStatus(SyncFileStatus::StatusSync));

        statusSpy.clear();
        fakeFolder.execUntilItemCompleted(QStringLiteral("D"));
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D/d0")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D")), SyncFileStatus(SyncFileStatus::StatusNone));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("D/d0")), SyncFileStatus(SyncFileStatus::StatusNone));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusDeleteUpDown() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().remove(QStringLiteral("B/b1"));
        fakeFolder.localModifier().remove(QStringLiteral("C/c1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        // Discovered as remotely removed, pending for local removal.
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("B/b2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("C/c2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void warningStatusForExcludedFile() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().addManualExclude(QStringLiteral("A/a1"));
        fakeFolder.syncEngine().addManualExclude(QStringLiteral("B"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("B/b1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QEXPECT_FAIL("", "csync will stop at ignored directories without traversing children, so we don't currently push the status for newly ignored children of an ignored directory.", Continue);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusExcluded));

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QEXPECT_FAIL("", "csync will stop at ignored directories without traversing children, so we don't currently push the status for newly ignored children of an ignored directory.", Continue);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QEXPECT_FAIL("", "csync will stop at ignored directories without traversing children, so we don't currently push the status for newly ignored children of an ignored directory.", Continue);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b2")), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        // Clears the exclude expr above
        fakeFolder.syncEngine().clearManualExcludes();
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void warningStatusForExcludedFile_CasePreserving() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().addManualExclude(QStringLiteral("B"));
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/a1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());

        fakeFolder.syncOnce();
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusExcluded));

        // Should still get the status for different casing on macOS and Windows.
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("a")),
            SyncFileStatus(Utility::fsCasePreserving() ? SyncFileStatus::StatusWarning : SyncFileStatus::StatusNone));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/A1")),
            SyncFileStatus(Utility::fsCasePreserving() ? SyncFileStatus::StatusError : SyncFileStatus::StatusNone));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("b")),
            SyncFileStatus(Utility::fsCasePreserving() ? SyncFileStatus::StatusExcluded : SyncFileStatus::StatusNone));
    }

    void parentsGetWarningStatusForError() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/a1"));
        fakeFolder.serverErrorPaths().append(QStringLiteral("B/b0"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        fakeFolder.localModifier().insert(QStringLiteral("B/b0"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusError));
        statusSpy.clear();

        // Remove the error and start a second sync, the blacklist should kick in
        fakeFolder.serverErrorPaths().clear();
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        // A/a1 and B/b0 should be on the black list for the next few seconds
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusError));
        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusError));
        statusSpy.clear();

        // Start a third sync, this time together with a real file to sync
        fakeFolder.localModifier().appendByte(QStringLiteral("C/c1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        // The root should show SYNC even though there is an error underneath,
        // since C/c1 is syncing and the SYNC status has priority.
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c1")), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a2")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        // Another sync after clearing the blacklist entry, everything should return to order.
        fakeFolder.syncEngine().journal()->wipeErrorBlacklistEntry(QStringLiteral("A/a1"));
        fakeFolder.syncEngine().journal()->wipeErrorBlacklistEntry(QStringLiteral("B/b0"));
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b0")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetWarningStatusForError_SibblingStartsWithPath() {
        // A is a parent of A/a1, but A/a is not even if it's a substring of A/a1
        FakeFolder fakeFolder{{QString{},{
            {QStringLiteral("A"), {
                {QStringLiteral("a"), 4},
                {QStringLiteral("a1"), 4}
            }}}}};
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/a1"));
        fakeFolder.localModifier().appendByte(QStringLiteral("A/a1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        // The SyncFileStatusTraker won't push any status for all of them, test with a pull.
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a")), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        fakeFolder.execUntilFinished();
        // We use string matching for paths in the implementation,
        // an error should affect only parents and not every path that starts with the problem path.
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a1")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(QStringLiteral("A/a")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
    }

    // Even for status pushes immediately following each other, macOS
    // can sometimes have 1s delays between updates, so make sure that
    // children are marked as OK before their parents do.
    void childOKEmittedBeforeParent() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().appendByte(QStringLiteral("B/b1"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        fakeFolder.remoteModifier().appendByte(QStringLiteral("C/c1"));
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.syncOnce();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QVERIFY(statusSpy.statusEmittedBefore(QStringLiteral("B/b1"), QStringLiteral("B")));
        QVERIFY(statusSpy.statusEmittedBefore(QStringLiteral("C/c1"), QStringLiteral("C")));
        QVERIFY(statusSpy.statusEmittedBefore(QStringLiteral("B"), QString()));
        QVERIFY(statusSpy.statusEmittedBefore(QStringLiteral("C"), QString()));
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("C/c1")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
    }

    void sharedStatus() {
        SyncFileStatus sharedUpToDateStatus(SyncFileStatus::StatusUpToDate);
        sharedUpToDateStatus.setShared(true);

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().insert(QStringLiteral("S/s0"));
        fakeFolder.remoteModifier().appendByte(QStringLiteral("S/s1"));
        fakeFolder.remoteModifier().insert(QStringLiteral("B/b3"));
        fakeFolder.remoteModifier().find(QStringLiteral("B/b3"))->extraDavProperties = "<oc:share-types><oc:share-type>0</oc:share-type></oc:share-types>";
        fakeFolder.remoteModifier().find(QStringLiteral("A/a1"))->isShared = true; // becomes shared
        fakeFolder.remoteModifier().find(QStringLiteral("A"), true); // change the etags of the parent

        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        // We don't care about the shared flag for the sync status,
        // Mac and Windows won't show it and we can't know it for new files.
        QCOMPARE(statusSpy.statusOf(QStringLiteral("S")).tag(), SyncFileStatus::StatusSync);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("S/s0")).tag(), SyncFileStatus::StatusSync);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("S/s1")).tag(), SyncFileStatus::StatusSync);

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("S")), sharedUpToDateStatus);
        QEXPECT_FAIL("", "We currently only know if a new file is shared on the second sync, after a PROPFIND.", Continue);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("S/s0")), sharedUpToDateStatus);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("S/s1")), sharedUpToDateStatus);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1")).shared(), false);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b3")), sharedUpToDateStatus);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), sharedUpToDateStatus);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void renameError() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.serverErrorPaths().append(QStringLiteral("A/a1"));
        fakeFolder.localModifier().rename(QStringLiteral("A/a1"), QStringLiteral("A/a1m"));
        fakeFolder.localModifier().rename(QStringLiteral("B/b1"), QStringLiteral("B/b1m"));
        QVERIFY(fakeFolder.applyLocalModificationsWithoutSync());
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();

        verifyThatPushMatchesPull(fakeFolder, statusSpy);

        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1m")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), statusSpy.statusOf(QStringLiteral("A/a1notexist")));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1m")), SyncFileStatus(SyncFileStatus::StatusSync));

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1m")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), statusSpy.statusOf(QStringLiteral("A/a1notexist")));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1m")), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        QVERIFY(!fakeFolder.syncOnce());
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        statusSpy.clear();
        QVERIFY(!fakeFolder.syncOnce());
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1m")), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A/a1")), statusSpy.statusOf(QStringLiteral("A/a1notexist")));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("A")), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QString()), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B")), SyncFileStatus(SyncFileStatus::StatusNone));
        QCOMPARE(statusSpy.statusOf(QStringLiteral("B/b1m")), SyncFileStatus(SyncFileStatus::StatusNone));
        statusSpy.clear();
    }

};

QTEST_GUILESS_MAIN(TestSyncFileStatusTracker)
#include "testsyncfilestatustracker.moc"
