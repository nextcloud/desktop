/*
 * SPDX-FileCopyrightText: 2026 Nextcloud GmbH and Nextcloud contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "propagateupload.h"
#include "syncenginetestutils.h"

#include <QJsonDocument>
#include <QJsonObject>

using namespace OCC;
using namespace Qt::StringLiterals;

class TestUploadLockState : public QObject
{
    Q_OBJECT

    static void setLock(SyncJournalFileRecord &record)
    {
        record._lockstate._locked = true;
        record._lockstate._lockOwnerId = u"owner"_s;
        record._lockstate._lockOwnerDisplayName = u"Owner"_s;
        record._lockstate._lockOwnerType = static_cast<qint64>(SyncFileItem::LockOwnerType::TokenLock);
        record._lockstate._lockEditorApp = u"editor"_s;
        record._lockstate._lockTime = QDateTime::currentSecsSinceEpoch();
        record._lockstate._lockTimeout = 3600;
        record._lockstate._lockToken = u"old-token"_s;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testLockIdentity_data()
    {
        QTest::addColumn<bool>("sameIdentity");
        QTest::addColumn<int>("ownerType");
        for (const auto sameIdentity : {false, true}) {
            for (const auto ownerType : {0, 1, 2}) {
                const auto name = u"same%1-owner%2"_s.arg(sameIdentity).arg(ownerType).toLatin1();
                QTest::newRow(name.constData()) << sameIdentity << ownerType;
            }
        }
    }

    void testLockIdentity()
    {
        QFETCH(bool, sameIdentity);
        QFETCH(int, ownerType);
        auto record = SyncJournalFileRecord{};
        record._fileId = "old-id";
        setLock(record);
        record._lockstate._lockOwnerType = ownerType;
        const auto item = SyncFileItem::fromSyncJournalFileRecord(record);
        item->_fileId = sameIdentity ? "old-id" : "new-id";

        item->updateLockStateFromDbRecord(record);

        const auto result = item->toSyncJournalFileRecordWithInode({});
        QCOMPARE(result._fileId, item->_fileId);
        QCOMPARE(result._lockstate._locked, sameIdentity);
        QCOMPARE(result._lockstate._lockToken, sameIdentity ? record._lockstate._lockToken : QString{});
        QCOMPARE(result._lockstate._lockOwnerId, sameIdentity ? record._lockstate._lockOwnerId : QString{});
        QCOMPARE(result._lockstate._lockOwnerDisplayName, sameIdentity ? record._lockstate._lockOwnerDisplayName : QString{});
        QCOMPARE(result._lockstate._lockEditorApp, sameIdentity ? record._lockstate._lockEditorApp : QString{});
        QCOMPARE(result._lockstate._lockOwnerType, sameIdentity ? qint64(ownerType) : qint64(0));
        QCOMPARE(result._lockstate._lockTime, sameIdentity ? record._lockstate._lockTime : qint64(0));
        QCOMPARE(result._lockstate._lockTimeout, sameIdentity ? record._lockstate._lockTimeout : qint64(0));
    }

    void testUploadReplacement_data()
    {
        QTest::addColumn<bool>("chunked");
        QTest::addColumn<bool>("sameIdentity");
        QTest::addColumn<bool>("journalEtagChanged");
        for (const auto chunked : {false, true}) {
            for (const auto sameIdentity : {false, true}) {
                for (const auto journalEtagChanged : {false, true}) {
                    const auto name = u"chunked%1-same%2-etag%3"_s.arg(chunked).arg(sameIdentity).arg(journalEtagChanged).toLatin1();
                    QTest::newRow(name.constData()) << chunked << sameIdentity << journalEtagChanged;
                }
            }
        }
    }

    void testUploadReplacement()
    {
        QFETCH(bool, chunked);
        QFETCH(bool, sameIdentity);
        QFETCH(bool, journalEtagChanged);
        auto folder = FakeFolder{FileInfo::A12_B12_C12_S12()};
        QVERIFY(folder.syncOnce());
        folder.account()->setCapabilities({{u"dav"_s, QVariantMap{{u"chunking"_s, "1.0"}}}});
        auto options = SyncOptions{};
        options._initialChunkSize = chunked ? 1 : 10000;
        options._targetChunkUploadDuration = std::chrono::milliseconds(0);
        folder.syncEngine().setSyncOptions(options);
        auto record = SyncJournalFileRecord{};
        QVERIFY(folder.syncJournal().getFileRecord(u"A/a1"_s, &record));
        QVERIFY(record.isValid());
        setLock(record);
        // FakePropfindReply serializes lockOwnerId as lock-owner-type.
        record._lockstate._lockOwnerId = u"2"_s;
        QVERIFY(folder.syncJournal().setFileRecord(record));
        auto remote = folder.remoteModifier().find(u"A/a1"_s);
        remote->lockState = FileInfo::LockState::FileLocked;
        remote->lockType = 2;
        remote->lockOwnerId = record._lockstate._lockOwnerId;
        remote->lockToken = record._lockstate._lockToken;
        remote->lockTime = record._lockstate._lockTime;
        remote->lockTimeout = record._lockstate._lockTimeout;
        const auto newId = sameIdentity ? record._fileId : QByteArray("99999999oc1x2y3z4w");
        auto completions = 0;
        auto uploadCondition = QByteArray{};
        folder.setServerOverride([&](auto op, const auto &request, QIODevice *data) -> QNetworkReply * {
            const auto isMove = request.attribute(QNetworkRequest::CustomVerbAttribute).toByteArray() == "MOVE";
            if ((!chunked && op == QNetworkAccessManager::PutOperation) || (chunked && isMove)) {
                uploadCondition = request.rawHeader("If");
                auto chunkRequest = request;
                // FakeChunkMoveReply only supports ETag conditions, not token locks.
                if (chunked) {
                    chunkRequest.setRawHeader("If", "<" + request.rawHeader("Destination") + "> ([\"" + remote->etag + "\"])");
                }
                auto uploaded = chunked ? FakeChunkMoveReply::perform(folder.uploadState(), folder.remoteModifier(), chunkRequest)
                                        : FakePutReply::perform(folder.remoteModifier(), request, data->readAll());
                if (!uploaded) {
                    return new FakeErrorReply(op, request, folder.networkAccessManager(), 412);
                }
                ++completions;
                uploaded->fileId = newId;
                if (journalEtagChanged) {
                    record._etag += "-lock-update";
                    if (!folder.syncJournal().setFileRecord(record)) {
                        return new FakeErrorReply(op, request, folder.networkAccessManager(), 500);
                    }
                }
                const auto reply = new FakeJsonReply(op, request, folder.networkAccessManager(), chunked ? 204 : 200);
                reply->setRawHeader("OC-FileID", newId);
                reply->setRawHeader("ETag", uploaded->etag);
                reply->setRawHeader("X-OC-MTime", "accepted");
                return reply;
            }
            return nullptr;
        });
        folder.localModifier().appendByte(u"A/a1"_s);
        QVERIFY(folder.syncOnce());
        QCOMPARE(completions, 1);
        QVERIFY(uploadCondition.contains("<opaquelocktoken:old-token>"));
        auto result = SyncJournalFileRecord{};
        QVERIFY(folder.syncJournal().getFileRecord(u"A/a1"_s, &result));
        QCOMPARE(result._fileId, newId);
        QCOMPARE(result._lockstate._locked, sameIdentity);
        QCOMPARE(result._lockstate._lockToken, sameIdentity ? record._lockstate._lockToken : QString{});
    }

    void testPollReplacement()
    {
        auto folder = FakeFolder{FileInfo::A12_B12_C12_S12()};
        QVERIFY(folder.syncOnce());
        auto record = SyncJournalFileRecord{};
        QVERIFY(folder.syncJournal().getFileRecord(u"A/a1"_s, &record));
        QVERIFY(record.isValid());
        setLock(record);
        QVERIFY(folder.syncJournal().setFileRecord(record));
        const auto item = SyncFileItem::fromSyncJournalFileRecord(record);
        folder.setServerOverride([&](auto op, const auto &request, QIODevice *) {
            return new FakeJsonReply(op,
                                     request,
                                     folder.networkAccessManager(),
                                     200,
                                     QJsonDocument(QJsonObject{{u"status"_s, "finished"}, {u"fileId"_s, "new-id"}, {u"ETag"_s, "new-etag"}}));
        });
        const auto job = new PollJob(folder.account(), u"/poll"_s, item, &folder.syncJournal(), folder.localPath(), this);
        auto finished = QSignalSpy(job, &PollJob::finishedSignal);
        job->start();
        QTRY_COMPARE(finished.size(), 1);
        QCOMPARE(item->_status, SyncFileItem::Success);
        QCOMPARE(item->_fileId, QByteArray("new-id"));
        QCOMPARE(item->_locked, SyncFileItem::LockStatus::UnlockedItem);
        QVERIFY(item->_lockToken.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestUploadLockState)
#include "testuploadlockstate.moc"
