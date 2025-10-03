/*
 *    This software is in the public domain, furnished "as is", without technical
 *    support, and with no warranty, express or implied, as to its usefulness for
 *    any purpose.
 *
 */

#include <QtTest>
#include "syncenginetestutils.h"
#include "csync_exclude.h"

using namespace OCC;

class StatusPushSpy : public QSignalSpy
{
    SyncEngine &_syncEngine;
public:
    StatusPushSpy(SyncEngine &syncEngine)
        : QSignalSpy(&syncEngine.syncFileStatusTracker(), SIGNAL(fileStatusChanged(const QString&, SyncFileStatus)))
        , _syncEngine(syncEngine)
    { }

    SyncFileStatus statusOf(const QString &relativePath) const {
        QFileInfo file(_syncEngine.localPath(), relativePath);
        // Start from the end to get the latest status
        for (int i = size() - 1; i >= 0; --i) {
            if (QFileInfo(at(i)[0].toString()) == file)
                return at(i)[1].value<SyncFileStatus>();
        }
        return {};
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

class TestSyncFileStatusTracker : public QObject
{
    Q_OBJECT

    void verifyThatPushMatchesPull(const FakeFolder &fakeFolder, const StatusPushSpy &statusSpy) {
        QString root = fakeFolder.localPath();
        QDirIterator it(root, QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString filePath = it.next().mid(root.size());
            SyncFileStatus pushedStatus = statusSpy.statusOf(filePath);
            if (pushedStatus != SyncFileStatus())
                QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(filePath), pushedStatus);
        }
    }

private slots:
    void parentsGetSyncStatusUploadDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().appendByte("B/b1");
        fakeFolder.remoteModifier().appendByte("C/c1");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("C/c1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("B/b2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("C/c2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C/c1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusNewFileUploadDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().insert("B/b0");
        fakeFolder.remoteModifier().insert("C/c0");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("C/c0"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("B/b1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("C/c1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C/c0"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusNewDirDownload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().mkdir("D");
        fakeFolder.remoteModifier().insert("D/d0");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D/d0"), SyncFileStatus(SyncFileStatus::StatusSync));

        fakeFolder.execUntilItemCompleted("D");
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D/d0"), SyncFileStatus(SyncFileStatus::StatusSync));

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("D"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("D/d0"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusNewDirUpload() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().mkdir("D");
        fakeFolder.localModifier().insert("D/d0");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D/d0"), SyncFileStatus(SyncFileStatus::StatusSync));

        fakeFolder.execUntilItemCompleted("D");
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("D/d0"), SyncFileStatus(SyncFileStatus::StatusSync));

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("D"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("D/d0"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetSyncStatusDeleteUpDown() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().remove("B/b1");
        fakeFolder.localModifier().remove("C/c1");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        // Discovered as remotely removed, pending for local removal.
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("B/b2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("C/c2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void warningStatusForExcludedFile() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().excludedFiles().addManualExclude("A/a1");
        fakeFolder.syncEngine().excludedFiles().addManualExclude("B");
        fakeFolder.localModifier().appendByte("A/a1");
        fakeFolder.localModifier().appendByte("B/b1");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QEXPECT_FAIL("", "csync will stop at ignored directories without traversing children, so we don't currently push the status for newly ignored children of an ignored directory.", Continue);
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusExcluded));

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QEXPECT_FAIL("", "csync will stop at ignored directories without traversing children, so we don't currently push the status for newly ignored children of an ignored directory.", Continue);
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QEXPECT_FAIL("", "csync will stop at ignored directories without traversing children, so we don't currently push the status for newly ignored children of an ignored directory.", Continue);
        QCOMPARE(statusSpy.statusOf("B/b2"), SyncFileStatus(SyncFileStatus::StatusExcluded));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        // Clears the exclude expr above
        fakeFolder.syncEngine().excludedFiles().clearManualExcludes();
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void warningStatusForExcludedFile_CasePreserving() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.syncEngine().excludedFiles().addManualExclude("B");
        fakeFolder.serverErrorPaths().append("A/a1");
        fakeFolder.localModifier().appendByte("A/a1");

        fakeFolder.syncOnce();
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("B"), SyncFileStatus(SyncFileStatus::StatusExcluded));

        // Should still get the status for different casing on macOS and Windows.
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("a"), SyncFileStatus(Utility::fsCasePreserving() ? SyncFileStatus::StatusWarning : SyncFileStatus::StatusNone));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/A1"), SyncFileStatus(Utility::fsCasePreserving() ? SyncFileStatus::StatusError : SyncFileStatus::StatusNone));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("b"), SyncFileStatus(Utility::fsCasePreserving() ? SyncFileStatus::StatusExcluded : SyncFileStatus::StatusNone));
    }

    void parentsGetWarningStatusForError() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.serverErrorPaths().append("A/a1");
        fakeFolder.serverErrorPaths().append("B/b0");
        fakeFolder.localModifier().appendByte("A/a1");
        fakeFolder.localModifier().insert("B/b0");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusError));
        statusSpy.clear();

        // Remove the error and start a second sync, the blacklist should kick in
        fakeFolder.serverErrorPaths().clear();
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        // A/a1 and B/b0 should be on the black list for the next few seconds
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusError));
        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusError));
        statusSpy.clear();

        // Start a third sync, this time together with a real file to sync
        fakeFolder.localModifier().appendByte("C/c1");
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        // The root should show SYNC even though there is an error underneath,
        // since C/c1 is syncing and the SYNC status has priority.
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("C/c1"), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a2"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C/c1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        // Another sync after clearing the blacklist entry, everything should return to order.
        fakeFolder.syncEngine().journal()->wipeErrorBlacklistEntry("A/a1");
        fakeFolder.syncEngine().journal()->wipeErrorBlacklistEntry("B/b0");
        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusSync));
        statusSpy.clear();
        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("A/a1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B/b0"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void parentsGetWarningStatusForError_SibblingStartsWithPath() {
        // A is a parent of A/a1, but A/a is not even if it's a substring of A/a1
        FakeFolder fakeFolder{{QString{},{
            {QStringLiteral("A"), {
                {QStringLiteral("a"), 4},
                {QStringLiteral("a1"), 4}
            }}}}};
        fakeFolder.serverErrorPaths().append("A/a1");
        fakeFolder.localModifier().appendByte("A/a1");

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        // The SyncFileStatusTraker won't push any status for all of them, test with a pull.
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a1"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a"), SyncFileStatus(SyncFileStatus::StatusUpToDate));

        fakeFolder.execUntilFinished();
        // We use string matching for paths in the implementation,
        // an error should affect only parents and not every path that starts with the problem path.
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a1"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(fakeFolder.syncEngine().syncFileStatusTracker().fileStatus("A/a"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
    }

    // Even for status pushes immediately following each other, macOS
    // can sometimes have 1s delays between updates, so make sure that
    // children are marked as OK before their parents do.
    void childOKEmittedBeforeParent() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.localModifier().appendByte("B/b1");
        fakeFolder.remoteModifier().appendByte("C/c1");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.syncOnce();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QVERIFY(statusSpy.statusEmittedBefore("B/b1", "B"));
        QVERIFY(statusSpy.statusEmittedBefore("C/c1", "C"));
        QVERIFY(statusSpy.statusEmittedBefore("B", ""));
        QVERIFY(statusSpy.statusEmittedBefore("C", ""));
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B/b1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("C/c1"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
    }

    void sharedStatus() {
        SyncFileStatus sharedUpToDateStatus(SyncFileStatus::StatusUpToDate);
        sharedUpToDateStatus.setShared(true);

        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.remoteModifier().insert("S/s0");
        fakeFolder.remoteModifier().appendByte("S/s1");
        fakeFolder.remoteModifier().insert("B/b3");
        fakeFolder.remoteModifier().find("B/b3")->extraDavProperties = "<oc:share-types><oc:share-type>0</oc:share-type></oc:share-types>";
        fakeFolder.remoteModifier().find("A/a1")->isShared = true; // becomes shared
        fakeFolder.remoteModifier().find("A", true); // change the etags of the parent

        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        // We don't care about the shared flag for the sync status,
        // Mac and Windows won't show it and we can't know it for new files.
        QCOMPARE(statusSpy.statusOf("S").tag(), SyncFileStatus::StatusSync);
        QCOMPARE(statusSpy.statusOf("S/s0").tag(), SyncFileStatus::StatusSync);
        QCOMPARE(statusSpy.statusOf("S/s1").tag(), SyncFileStatus::StatusSync);

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("S"), sharedUpToDateStatus);
        QEXPECT_FAIL("", "We currently only know if a new file is shared on the second sync, after a PROPFIND.", Continue);
        QCOMPARE(statusSpy.statusOf("S/s0"), sharedUpToDateStatus);
        QCOMPARE(statusSpy.statusOf("S/s1"), sharedUpToDateStatus);
        QCOMPARE(statusSpy.statusOf("B/b1").shared(), false);
        QCOMPARE(statusSpy.statusOf("B/b3"), sharedUpToDateStatus);
        QCOMPARE(statusSpy.statusOf("A/a1"), sharedUpToDateStatus);

        QCOMPARE(fakeFolder.currentLocalState(), fakeFolder.currentRemoteState());
    }

    void renameError() {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        fakeFolder.serverErrorPaths().append("A/a1");
        fakeFolder.localModifier().rename("A/a1", "A/a1m");
        fakeFolder.localModifier().rename("B/b1", "B/b1m");
        StatusPushSpy statusSpy(fakeFolder.syncEngine());

        fakeFolder.scheduleSync();
        fakeFolder.execUntilBeforePropagation();

        verifyThatPushMatchesPull(fakeFolder, statusSpy);

        QCOMPARE(statusSpy.statusOf("A/a1m"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("A/a1"), statusSpy.statusOf("A/a1notexist"));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusSync));
        QCOMPARE(statusSpy.statusOf("B/b1m"), SyncFileStatus(SyncFileStatus::StatusSync));

        fakeFolder.execUntilFinished();
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf("A/a1m"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf("A/a1"), statusSpy.statusOf("A/a1notexist"));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        QCOMPARE(statusSpy.statusOf("B/b1m"), SyncFileStatus(SyncFileStatus::StatusUpToDate));
        statusSpy.clear();

        QVERIFY(!fakeFolder.syncOnce());
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        statusSpy.clear();
        QVERIFY(!fakeFolder.syncOnce());
        verifyThatPushMatchesPull(fakeFolder, statusSpy);
        QCOMPARE(statusSpy.statusOf("A/a1m"), SyncFileStatus(SyncFileStatus::StatusError));
        QCOMPARE(statusSpy.statusOf("A/a1"), statusSpy.statusOf("A/a1notexist"));
        QCOMPARE(statusSpy.statusOf("A"), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf(""), SyncFileStatus(SyncFileStatus::StatusWarning));
        QCOMPARE(statusSpy.statusOf("B"), SyncFileStatus(SyncFileStatus::StatusNone));
        QCOMPARE(statusSpy.statusOf("B/b1m"), SyncFileStatus(SyncFileStatus::StatusNone));
        statusSpy.clear();
    }

};

QTEST_GUILESS_MAIN(TestSyncFileStatusTracker)
#include "testsyncfilestatustracker.moc"
