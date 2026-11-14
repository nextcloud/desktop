/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: CC0-1.0
 *
 * This software is in the public domain, furnished "as is", without technical
 * support, and with no warranty, express or implied, as to its usefulness for
 * any purpose.
 *
 * Regression tests for the Team Folder data-loss behavior tracked in
 * nextcloud/desktop#11114.
 *
 * Thesis under test: the client must not resolve AMBIGUOUS server state (an
 * incomplete-but-successful 207 listing, an empty child listing, a permission or
 * share loss) into a LOCAL DELETE. Absence from a successful PROPFIND is not
 * authoritative proof of deletion.
 *
 * The two reproducing cases are encoded as expected-failures (QEXPECT_FAIL) so
 * CI stays green while the defect is documented precisely; each converts to a
 * hard pass when the corresponding fix lands (remove the QEXPECT_FAIL). The
 * remaining invariants are present as QSKIP placeholders describing the harness
 * capability each still needs.
 */

#include "syncenginetestutils.h"
#include <QFileInfo>
#include <QtTest>
#include <syncengine.h>

using namespace OCC;

namespace
{

bool isPropfind(const QNetworkRequest &req)
{
    return req.attribute(QNetworkRequest::CustomVerbAttribute).toString() == QStringLiteral("PROPFIND");
}

} // namespace

// A well-formed 207 that OMITS one known top-level directory from the root
// listing, while that directory still EXISTS on the server. This models a
// top-level collection becoming invisible in a parent listing (for example when
// a groupfolder is disabled server-side) without its data having been deleted.
//
// Implementation mirrors MissingPermissionsPropfindReply in testremotediscovery:
// let the base build the full payload, then surgically remove one <d:response>.
struct OmitTopLevelEntryPropfindReply : FakePropfindReply {
    OmitTopLevelEntryPropfindReply(FileInfo &remoteRootFileInfo,
                                   const QString &omit,
                                   QNetworkAccessManager::Operation op,
                                   const QNetworkRequest &request,
                                   QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent)
    {
        // The omitted folder's self-response has an href ending in "/<name>/".
        const QByteArray hrefMarker = QByteArrayLiteral("/") + omit.toUtf8() + QByteArrayLiteral("/</d:href>");
        const int hrefPos = payload.indexOf(hrefMarker);
        QVERIFY(hrefPos > 0); // the entry we intend to drop must actually be present
        const int respStart = payload.lastIndexOf(QByteArrayLiteral("<d:response>"), hrefPos);
        int respEnd = payload.indexOf(QByteArrayLiteral("</d:response>"), hrefPos);
        QVERIFY(respStart >= 0 && respEnd > respStart);
        respEnd += int(sizeof("</d:response>") - 1);
        payload.remove(respStart, respEnd - respStart);
    }
};

// A well-formed 207 for a directory that returns ZERO children, even though the
// directory is known (in the DB) to have children. Models a transient/partial
// server listing that must NOT be read as "delete everything inside".
struct EmptyChildrenPropfindReply : FakePropfindReply {
    EmptyChildrenPropfindReply(FileInfo &remoteRootFileInfo, QNetworkAccessManager::Operation op, const QNetworkRequest &request, QObject *parent)
        : FakePropfindReply(remoteRootFileInfo, op, request, parent)
    {
        // Keep the first <d:response> (the collection itself); drop the rest.
        const int firstEnd = payload.indexOf(QByteArrayLiteral("</d:response>"));
        QVERIFY(firstEnd > 0);
        const int bodyStart = firstEnd + int(sizeof("</d:response>") - 1);
        const int msEnd = payload.indexOf(QByteArrayLiteral("</d:multistatus>"), bodyStart);
        QVERIFY(msEnd > bodyStart);
        payload.remove(bodyStart, msEnd - bodyStart);
    }
};

class TestDataLoss : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        OCC::Logger::instance()->setLogFlush(true);
        OCC::Logger::instance()->setLogDebug(true);
        QStandardPaths::setTestModeEnabled(true);
    }

    // A successful root 207 omits top-level dir "S" while it still exists on the
    // server. The client must treat this as a no-op, not as a deletion of S's
    // entire local subtree.
    void no_delete_on_incomplete_listing()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QVERIFY(fakeFolder.syncOnce());
        auto stateBefore = fakeFolder.currentLocalState();
        QVERIFY(stateBefore.find("S/s1")); // precondition: S has content locally

        // "S" is omitted from the root listing only; it is NOT removed from the
        // server model (its own listing would still succeed). The root listing is
        // the first PROPFIND of a sync; tamper with only that one so we never
        // corrupt a subfolder listing by accident.
        bool omittedRoot = false;
        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *) -> QNetworkReply * {
            if (isPropfind(req) && !omittedRoot) {
                omittedRoot = true;
                return new OmitTopLevelEntryPropfindReply(fakeFolder.remoteModifier(), QStringLiteral("S"), op, req, this);
            }
            return nullptr; // every other request served normally
        });

        QVERIFY(fakeFolder.syncOnce());

        // An incomplete (but successful) listing must be a local no-op. Today the
        // client reads the omission as an authoritative deletion and removes S's
        // entire local subtree. Expected-fail until that is fixed (#11114).
        QEXPECT_FAIL("", "#11114: a collection omitted from a successful PROPFIND is treated as an authoritative deletion", Abort);
        QVERIFY2(fakeFolder.currentLocalState().find("S/s1"), "S/s1 was deleted locally from an incomplete but successful server listing");

        // When fixed, an incomplete listing is a strict local no-op.
        QCOMPARE(fakeFolder.currentLocalState(), stateBefore);
    }

    // A known directory returns zero children under a successful 207. Must not
    // mass-delete that directory's local contents.
    void empty_children_is_not_delete_all()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        QVERIFY(fakeFolder.syncOnce());
        auto stateBefore = fakeFolder.currentLocalState();
        QVERIFY(stateBefore.find("A/a1"));

        fakeFolder.setServerOverride([&](QNetworkAccessManager::Operation op, const QNetworkRequest &req, QIODevice *) -> QNetworkReply * {
            // Only the listing OF "A" returns empty; root and others are normal.
            if (isPropfind(req) && req.url().path().endsWith(QStringLiteral("/A/"))) {
                return new EmptyChildrenPropfindReply(fakeFolder.remoteModifier(), op, req, this);
            }
            return nullptr;
        });

        QVERIFY(fakeFolder.syncOnce());

        QVERIFY2(fakeFolder.currentLocalState().find("A/a1"), "A/a1 mass-deleted from an empty child listing");
        QCOMPARE(fakeFolder.currentLocalState(), stateBefore);
    }

    // Even when the server deletion is entirely LEGITIMATE, a sync-excluded,
    // local-only child (".git/") under the removed directory must SURVIVE.
    // Excluded content was never part of the server namespace, so a server-side
    // removal has no authority to delete it: the recursive remover must not unlink
    // entries under the directory without consulting the exclude list / database.
    void excluded_local_only_survives_ancestor_removal()
    {
        FakeFolder fakeFolder{FileInfo::A12_B12_C12_S12()};
        // ".git" is excluded from synchronization (as in a typical sync-exclude.lst).
        fakeFolder.syncEngine().excludedFiles().addManualExclude(QStringLiteral(".git"));

        // A local-only, excluded directory lives under synchronized folder "A".
        // Being excluded, it is never uploaded and must never be touched by sync.
        fakeFolder.localModifier().mkdir(QStringLiteral("A/.git"));
        fakeFolder.localModifier().insert(QStringLiteral("A/.git/config"), 128, 'g');
        fakeFolder.localModifier().insert(QStringLiteral("A/.git/HEAD"), 32, 'h');

        QVERIFY(fakeFolder.syncOnce());
        // Check TRUE on-disk state: currentLocalState() walks without QDir::Hidden,
        // so it cannot observe a dotfile like .git -- we must stat the real path.
        const auto onDisk = [&](const char *rel) {
            return QFileInfo::exists(fakeFolder.localPath() + QString::fromLatin1(rel));
        };
        // Preconditions: excluded content is local-only (present on disk, absent remote).
        QVERIFY(onDisk("A/.git/config"));
        QVERIFY(!fakeFolder.currentRemoteState().find("A/.git"));

        // Force steady-state INCREMENTAL (DB-only) local discovery for the deletion
        // sync: "A" is not re-scanned from the filesystem, so the excluded .git
        // (never in the DB) is invisible to discovery and the _childIgnored guard
        // never fires. Without this, a full local scan sees .git, the guard fires,
        // and A is correctly preserved -- which is why this only occurs in
        // steady-state operation, not on a freshly-scanned folder.
        fakeFolder.syncEngine().setLocalDiscoveryOptions(LocalDiscoveryStyle::DatabaseAndFilesystem, {});

        // The server LEGITIMATELY removes directory "A" (a real, intentional delete).
        fakeFolder.remoteModifier().remove("A");
        QVERIFY(fakeFolder.syncOnce());

        // The SYNCHRONIZED contents of A are correctly gone from disk...
        QVERIFY2(!onDisk("A/a1"), "synchronized A/a1 should have been removed by the legitimate delete");
        // ...but the EXCLUDED, local-only content MUST survive the recursive remove.
        // Today it does not. Expected-fail until the recursive remover consults the
        // exclude list / database before unlinking (#11114).
        QEXPECT_FAIL("", "#11114: recursive removal destroys excluded local-only content under DB-only discovery", Abort);
        QVERIFY2(onDisk("A/.git/config"), "excluded local-only A/.git/config was destroyed by recursive removal of A");
        QVERIFY2(onDisk("A/.git/HEAD"), "excluded local-only A/.git/HEAD was destroyed by recursive removal of A");
    }

    // A file queued for Down-REMOVE that is modified locally between discovery and
    // propagation must be preserved, never deleted.
    void edit_during_window_survives()
    {
        QSKIP(
            "TODO: needs a test seam to mutate the local file between the "
            "discovery and propagation phases (hook aboutToPropagate / a "
            "single-item stall). PropagateLocalRemove lacks the verifyFileUnchanged "
            "guard that other propagator paths have.");
    }

    // An entire known mount disappearing (even below the bulk-delete threshold,
    // with other folders still syncing) should trigger a safe abort/defer
    // regardless of promptDeleteAllFiles.
    void whole_mount_vanish_aborts()
    {
        QSKIP(
            "TODO: needs a multi-folder FakeFolder set so one mount can vanish "
            "while others sync, plus assertion on the safe-abort signal. The "
            "bulk-delete brake is default-off, never fires headless, and uses a "
            "cause-blind flat count.");
    }

    // Server strips CanRead on a known item => local must be RETAINED
    // (dehydrate/ignore), not deleted. Related to #8831.
    void canread_loss_is_not_delete()
    {
        QSKIP(
            "TODO: set FileInfo.permissions to drop the 'R' bit on a known item "
            "via a tampered PROPFIND and assert local retention. checkPermissions "
            "only guards direction==Up.");
    }

    // Deleting more than X% of a folder's known contents should trip the
    // bulk-delete brake even under the default flat-count threshold.
    void fraction_threshold()
    {
        QSKIP(
            "TODO: depends on a per-mount/fraction brake existing (it does not "
            "yet). Fails once the flat >100 count is replaced with fraction logic.");
    }
};

QTEST_GUILESS_MAIN(TestDataLoss)
#include "testdataloss.moc"
